#include "ilua.h"
#include "base/types.h"
#include <windows.h>

void luaL_printf(luaL_Buffer *B, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  size_t size = _vscprintf(fmt, ap);
  char* buf = luaL_prepbuffsize(B, size + 1);
  vsprintf(buf, fmt, ap);
  luaL_addsize(B, size);
}

namespace ilua
{

void openlib(lua_State* L, char const* name)
{
  lua_getglobal(L, name);
  if (!lua_istable(L, -1))
  {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setglobal(L, name);
  }
}
void bindmethod(lua_State* L, char const* name, lua_CFunction func)
{
  lua_pushcfunction(L, func);
  lua_setfield(L, -2, name);
}

void setuservalue(lua_State* L, int pos)
{
  pos = lua_absindex(L, pos);
  lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_EXTRA);
  lua_pushvalue(L, pos);
  lua_pushvalue(L, -3);
  lua_settable(L, -3);
  lua_pop(L, 2);
}
void getuservalue(lua_State* L, int pos)
{
  pos = lua_absindex(L, pos);
  lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_EXTRA);
  lua_pushvalue(L, pos);
  lua_gettable(L, -2);
  lua_remove(L, -2);
}

void lcall(lua_State* L, int args, int ret)
{
  Thread* t = engine(L)->current_thread();
  if (t) t->lock();
  lua_call(L, args, ret);
  if (t) t->unlock();
}
static int nullyield(lua_State* L)
{
  int ctx = 0;
  lua_getctx(L, &ctx);
  return lua_gettop(L) - ctx;
}
int ycall(lua_State* L, int args, int ret)
{
  int top = lua_gettop(L) - args - 1;
  lua_callk(L, args, ret, top, nullyield);
  return lua_gettop(L) - top;
}

int checkoption(lua_State* L, int narg, const char* def, const char* const lst[])
{
  if (lua_type(L, narg) == LUA_TNUMBER)
  {
    int value = lua_tointeger(L, narg);
    int size = 0;
    while (lst[size]) size++;
    if (value >= 0 && value < size)
      return value;
  }
  return luaL_checkoption(L, narg, def, lst);
}

int totable(lua_State* L, int pos)
{
  pos = lua_absindex(L, pos);
  if (!lua_getmetatable(L, pos))
    return 0;
  // check that __newindex is still a cfunction
  lua_getfield(L, -1, "__newindex");
  if (!lua_iscfunction(L, -1))
  {
    if (lua_istable(L, -1))
    {
      // already converted
      lua_remove(L, -2);
      return 1;
    }
    lua_pop(L, 2);
    return 0;
  }
  lua_pop(L, 1);
  // create new metatable
  lua_newtable(L);
  // stack = (OLDMETA) (NEWMETA)
  // copy name/parent
  lua_rawgeti(L, -2, 0);
  lua_rawseti(L, -2, 0);
  lua_rawgeti(L, -2, 1);
  lua_rawseti(L, -2, 1);
  // create values table
  lua_newtable(L);
  // create metatable for values
  lua_newtable(L);
  // stack = (OLDMETA) (NEWMETA) (VALUES) (VALMETA)
  // copy old __index to VALMETA
  lua_getfield(L, -4, "__index");
  lua_setfield(L, -2, "__index");
  lua_setmetatable(L, -2);
  // stack = (OLDMETA) (NEWMETA) (VALUES)
  lua_pushvalue(L, -1);
  lua_pushvalue(L, -1);
  // stack = (OLDMETA) (NEWMETA) (VALUES) (VALUES) (VALUES)
  lua_replace(L, -5);
  // stack = (VALUES) (NEWMETA) (VALUES) (VALUES)
  lua_setfield(L, -3, "__index");
  lua_setfield(L, -2, "__newindex");
  // stack = (VALUES) (NEWMETA)
  lua_setmetatable(L, pos);
  return 1;
}
static int __newindex(lua_State* L)
{
  if (totable(L, 1))
  {
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_rawset(L, -3);
    lua_pop(L, 1);
  }
  return 0;
}

bool __newtype(lua_State* L, char const* name, char const* parent)
{
  bool create = false;
  lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_META);
  lua_getfield(L, -1, name);
  if (lua_isnil(L, -1))
  {
    create = true;
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, name);

    lua_pushstring(L, name);
    lua_rawseti(L, -2, 0);
    if (parent)
    {
      lua_pushstring(L, parent);
      lua_rawseti(L, -2, 1);
    }

    lua_pushcfunction(L, __newindex);
    lua_setfield(L, -2, "__newindex");

    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, "__index");

    if (parent)
    {
      lua_getfield(L, -3, parent);
      if (lua_istable(L, -1))
      {
        lua_getfield(L, -1, "__index");
        lua_remove(L, -2);
      }
      if (lua_istable(L, -1))
      {
        lua_pushnil(L);
        while (lua_next(L, -2))
        {
          if (lua_type(L, -2) == LUA_TSTRING)
            lua_setfield(L, -4, lua_tostring(L, -2));
          else
            lua_pop(L, 1);
        }
      }
      lua_pop(L, 1);
    }
  }
  else
    lua_getfield(L, -1, "__index");
  lua_remove(L, -3);
  return create;
}

bool iskindof(lua_State* L, char const* name, char const* base)
{
  if (!strcmp(name, base))
    return true;
  int top = lua_gettop(L);
  lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_META);
  while (name && strcmp(name, base))
  {
    lua_getfield(L, top + 1, name);
    if (!lua_istable(L, -1))
    {
      lua_settop(L, top);
      return false;
    }
    lua_rawgeti(L, -1, 1);
    name = lua_tostring(L, -1);
  }
  lua_settop(L, top);
  return (name != NULL);
}
bool iskindof(lua_State* L, int pos, char const* name)
{
  if (!lua_isuserdata(L, pos) || !lua_getmetatable(L, pos))
    return false;
  lua_rawgeti(L, -1, 0);
  if (lua_isstring(L, -1) && !strcmp(name, lua_tostring(L, -1)))
  {
    lua_pop(L, 2);
    return true;
  }
  lua_pop(L, 1);
  lua_rawgeti(L, -1, 1);
  bool result = (lua_isstring(L, -1) && iskindof(L, lua_tostring(L, -1), name));
  lua_pop(L, 2);
  return result;
}

void pushobject(lua_State* L, Object* obj)
{
  if (obj)
  {
    lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_BIND);
    lua_rawgetp(L, -1, obj);
    lua_remove(L, -2);
  }
  else
    lua_pushnil(L);
}

int Object::addref()
{
  if (InterlockedIncrement(&ref) == 1)
  {
    lua_State* L = e->lock();
    lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_BIND);
    lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_XREF);
    lua_rawgetp(L, -2, this);
    lua_rawsetp(L, -2, this);
    lua_pop(L, 2);
    e->unlock();
  }
  return ref;
}
int Object::release()
{
  if (InterlockedDecrement(&ref) == 0)
  {
    lua_State* L = e->lock();
    lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_XREF);
    lua_pushnil(L);
    lua_rawsetp(L, -2, this);
    lua_pop(L, 1);
    e->unlock();
  }
  return ref;
}

Engine* engine(lua_State* L)
{
  lua_getfield(L, LUA_REGISTRYINDEX, "ilua_engine");
  Engine* e = (Engine*) lua_touserdata(L, -1);
  lua_pop(L, 1);
  return e;
}

}

void* operator new(size_t count, lua_State* L, char const* name)
{
  void* ptr = lua_newuserdata(L, count);
  if (ilua::iskindof(L, name, "object"))
  {
    ((ilua::Object*) ptr)->e = ilua::engine(L);

    lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_BIND);
    lua_pushvalue(L, -2);
    lua_rawsetp(L, -2, ptr);
    lua_pop(L, 1);

    //lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_XREF);
    //lua_pushvalue(L, -2);
    //lua_rawsetp(L, -2, ptr);
    //lua_pop(L, 1);
  }
  lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_META);
  lua_getfield(L, -1, name);
  if (lua_istable(L, -1))
    lua_setmetatable(L, -3);
  else
    lua_pop(L, 1);
  lua_pop(L, 1);
  return ptr;
}
void operator delete(void* ptr, lua_State* L, char const* name)
{
  if (ilua::iskindof(L, name, "object"))
  {
    lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_BIND);
    lua_pushnil(L);
    lua_rawsetp(L, -2, ptr);
    lua_pop(L, 1);

    //lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_XREF);
    //lua_pushnil(L);
    //lua_rawsetp(L, -2, ptr);
    //lua_pop(L, 1);
  }
  lua_pushnil(L);
  lua_setmetatable(L, -1);
}
