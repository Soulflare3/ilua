#include "engine.h"

namespace api
{

static int co_resume(lua_State* L)
{
  Thread* thread = engine(L)->current_thread();
  luaL_checktype(L, 1, LUA_TTHREAD);
  lua_State* S = lua_tothread(L, 1);
  if (lua_status(S) == LUA_OK && lua_gettop(S) == 0)
  {
    lua_pushnil(L);
    return 1;
  }
  int nargs = lua_gettop(L) - 1;
  if (!lua_checkstack(S, nargs))
    luaL_error(L, "too many arguments to resume");
  lua_xmove(L, S, nargs);
  if (lua_gettop(S) > nargs)
    nargs = lua_gettop(S) - 1;
  thread->setco(S);
  int status = lua_resume(S, L, nargs);
  int nres = lua_gettop(S);
  if (status == LUA_YIELD)
  {
    if (nres > 0)
    {
      if ((uint32) lua_touserdata(S, -1) == CO_MAGIC)
      {
        lua_pop(S, 1);
        nres--;
      }
      else
      {
        lua_settop(S, 0);
        return lua_yieldk(L, 0, 0, co_resume);
      }
    }
    else
      return lua_yieldk(L, 0, 0, co_resume);
  }
  thread->setco(L);
  if (status == LUA_OK || status == LUA_YIELD)
  {
    if (!lua_checkstack(L, nres))
      luaL_error(L, "too many results to resume");
    lua_xmove(S, L, nres);
    return nres;
  }
  else
    return lua_error(L);
}
static int co_wrap_func(lua_State* L)
{
  lua_pushvalue(L, lua_upvalueindex(1));
  if (lua_gettop(L) > 1)
    lua_insert(L, 1);
  return co_resume(L);
}
static int co_create(lua_State *L)
{
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lua_State* S = lua_newthread(L);
  lua_insert(L, 1);
  lua_xmove(L, S, lua_gettop(L) - 1);
  return 1;
}
static int co_wrap(lua_State* L)
{
  co_create(L);
  lua_pushcclosure(L, co_wrap_func, 1);
  return 1;
}

static int co_yield(lua_State* L)
{
  lua_pushlightuserdata(L, (void*) CO_MAGIC);
  return lua_yield(L, lua_gettop(L));
}

static int co_status(lua_State *L)
{
  lua_State* S = lua_tothread(L, 1);
  luaL_argcheck(L, S, 1, "coroutine expected");
  if (L == S)
    lua_pushliteral(L, "running");
  else
  {
    switch (lua_status(S))
    {
    case LUA_YIELD:
      lua_pushliteral(L, "suspended");
      break;
    case LUA_OK:
      {
        lua_Debug ar;
        if (lua_getstack(S, 0, &ar) > 0)
          lua_pushliteral(L, "normal");
        else if (lua_gettop(S) == 0)
          lua_pushliteral(L, "dead");
        else
          lua_pushliteral(L, "suspended");
      }
      break;
    default:
      lua_pushliteral(L, "dead");
    }
  }
  return 1;
}

void bind_co(lua_State* L)
{
  ilua::openlib(L, "co");
  ilua::bindmethod(L, "create", co_create);
  ilua::bindmethod(L, "resume", co_resume);
  ilua::bindmethod(L, "yield", co_yield);
  ilua::bindmethod(L, "wrap", co_wrap);
  ilua::bindmethod(L, "status", co_status);
  lua_pop(L, 1);
}

}
