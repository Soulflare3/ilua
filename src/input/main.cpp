#include <windows.h>
#include <lua/lua.hpp>

#include "ilua/ilua.h"
#include "base/thread.h"
#include "module.h"
#include "list.h"
#include "keylist.h"
#include "base/dictionary.h"
#include "base/utils.h"

static int input_hook(lua_State* L)
{
  if (!lua_isnil(L, 1))
    luaL_argcheck(L, lua_isfunction(L, 1), 1, "function expected");
  InputModule* m = InputModule::get(L);
  lua_pushvalue(L, 1);
  int ref = (lua_isnil(L, 1) ? 0 : luaL_ref(L, LUA_REGISTRYINDEX));
  m->sethook(ref);
  return 0;
}
static int input_bind(lua_State* L)
{
  InputModule* m = InputModule::get(L);

  int limit = luaL_optinteger(L, 3, 0);

  uint32 key = 0;
  if (lua_type(L, 1) == LUA_TSTRING)
  {
    char const* str = lua_tostring(L, 1);
    key = InputList::parsekey(str, m->getkeymap());
  }
  else
    key = luaL_checkinteger(L, 1);
  if ((key & 0xFF) == 0)
  {
    lua_pushnil(L);
    return 1;
  }
  int handler = 0;
  if (lua_isfunction(L, 2))
  {
    lua_pushvalue(L, 2);
    handler = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  m->bind(key, handler, limit);
  lua_pushinteger(L, key);
  return 1;
}

static int input_sendex(lua_State* L, bool text)
{
  int top = lua_gettop(L);
  for (int i = 1; i <= top; i++)
    if (lua_type(L, i) != LUA_TNUMBER && lua_type(L, i) != LUA_TSTRING)
      luaL_argerror(L, i, "string or integer expected");
  InputModule* m = InputModule::get(L);
  InputList inp;
  for (int i = 1; i <= top; i++)
  {
    if (lua_type(L, i) == LUA_TNUMBER)
      inp.addkey(lua_tointeger(L, i));
    else if (text)
    {
      size_t length;
      char const* str = lua_tolstring(L, i, &length);
      inp.addtext(str, length);
    }
    else
      inp.addseq(lua_tostring(L, i), m->getkeymap());
  }
  inp.send();
  return 0;
}
static int input_send(lua_State* L)
{
  return input_sendex(L, false);
}
static int input_sendtext(lua_State* L)
{
  return input_sendex(L, true);
}
static int key_name(lua_State* L)
{
  int i = luaL_checkinteger(L, 1);
  if (i < 0) i = -i;
  if (i < 256 && keynames[i])
    lua_pushstring(L, keynames[i]);
  else
    lua_pushnil(L);
  return 1;
}
static int input_ontext(lua_State* L)
{
  char const* str = luaL_checkstring(L, 1);
  luaL_argcheck(L, lua_isfunction(L, 2), 2, "function expected");
  InputModule* m = InputModule::get(L);
  lua_pushvalue(L, 2);
  m->addsequence(str, true, luaL_ref(L, LUA_REGISTRYINDEX));
  return 0;
}
static int input_onsequence(lua_State* L)
{
  char const* str = luaL_checkstring(L, 1);
  luaL_argcheck(L, lua_isfunction(L, 2), 2, "function expected");
  InputModule* m = InputModule::get(L);
  lua_pushvalue(L, 2);
  m->addsequence(str, false, luaL_ref(L, LUA_REGISTRYINDEX));
  return 0;
}
static int input_pressed(lua_State* L)
{
  int key;
  if (lua_type(L, 1) == LUA_TNUMBER)
    key = (lua_tointeger(L, 1) & 0xFF);
  else if (lua_type(L, 1) == LUA_TSTRING)
  {
    InputModule* m = InputModule::get(L);
    key = m->nametokey(lua_tostring(L, 1));
  }
  else
    luaL_argerror(L, 1, "string or integer expected");
  lua_pushboolean(L, (key != 0) && (GetAsyncKeyState(key) & 0x8000));
  return 1;
}

static int input_list_addkeys(lua_State* L)
{
  InputList* list = ilua::checkobject<InputList>(L, 1, "input.list");
  InputModule* m = InputModule::get(L);
  int top = lua_gettop(L);
  for (int i = 2; i <= top; i++)
  {
    if (lua_type(L, i) == LUA_TNUMBER)
      list->addkey(lua_tointeger(L, i));
    else
      list->addseq(lua_tostring(L, i), m->getkeymap());
  }
  return 0;
}

static int input_list_addtext(lua_State* L)
{
  InputList* list = ilua::checkobject<InputList>(L, 1, "input.list");
  int top = lua_gettop(L);
  for (int i = 2; i <= top; i++)
  {
    if (lua_type(L, i) == LUA_TNUMBER)
      list->addkey(lua_tointeger(L, i));
    else
    {
      size_t length;
      char const* str = lua_tolstring(L, i, &length);
      list->addtext(str, length);
    }
  }
  return 0;
}
static int input_list_randomize(lua_State* L)
{
  InputList* list = ilua::checkobject<InputList>(L, 1, "input.list");
  list->randomize(luaL_optnumber(L, 2, 0.5f), lua_toboolean(L, 3));
  return 0;
}
static int input_list_length(lua_State* L)
{
  InputList* list = ilua::checkobject<InputList>(L, 1, "input.list");
  lua_pushinteger(L, list->length());
  return 1;
}
static int input_list_duration(lua_State* L)
{
  InputList* list = ilua::checkobject<InputList>(L, 1, "input.list");
  lua_pushnumber(L, double(list->duration()) / 1000);
  return 1;
}
static int input_list_addmove(lua_State* L)
{
  InputList* list = ilua::checkobject<InputList>(L, 1, "input.list");
  if (lua_toboolean(L, 4))
    list->addposrel(luaL_checknumber(L, 2), luaL_checknumber(L, 3));
  else
    list->addpos(luaL_checknumber(L, 2), luaL_checknumber(L, 3));
  return 0;
}
static int input_list_delay(lua_State* L)
{
  InputList* list = ilua::checkobject<InputList>(L, 1, "input.list");
  list->delay(int(luaL_checknumber(L, 2) * 1000));
  return 0;
}
struct SlowInputList
{
  InputList* list;
  int pos;
  int time;
  int gettime(int i)
  {
    return (list->duration() ? list->itemtime(i) * time / list->duration() : time / list->length());
  }
};
static int input_list_send_cont(lua_State* L)
{
  int ctx = lua_gettop(L);
  lua_getctx(L, &ctx);
  SlowInputList* sil = (SlowInputList*) lua_touserdata(L, ctx);
  if (sil->pos < sil->list->length())
  {
    if (sil->list->duration())
      sil->pos = sil->list->sendex(sil->pos);
    else
    {
      sil->list->send(sil->pos, sil->pos + 1);
      sil->pos++;
    }
  }
  if (sil->pos < sil->list->length())
    return ilua::engine(L)->current_thread()->sleep(sil->gettime(sil->pos), input_list_send_cont, ctx);
  else
    return 0;
}
static int input_list_send(lua_State* L)
{
  double t = luaL_optinteger(L, 2, -1);
  InputList* list = ilua::checkobject<InputList>(L, 1, "input.list");
  if (t == 0 || (t < 0 && list->duration() == 0))
    list->send();
  else if (list->length())
  {
    SlowInputList* sil = ilua::newstruct<SlowInputList>(L);
    sil->list = list;
    sil->pos = 0;
    sil->time = (t < 0 ? list->duration() : int(t * 1000));
    if (list->duration() && list->itemtime(0) == 0)
      return input_list_send_cont(L);
    else
      return ilua::engine(L)->current_thread()->sleep(sil->gettime(0), input_list_send_cont, lua_gettop(L));
  }
  return 0;
}
static int input_list_recordstart(lua_State* L)
{
  ilua::checkobject<InputList>(L, 1, "input.list");
  InputModule* m = InputModule::get(L);
  return m->recordstart(L, 1);
}
static int input_list_recordstop(lua_State* L)
{
  ilua::checkobject<InputList>(L, 1, "input.list");
  InputModule* m = InputModule::get(L);
  return m->recordstop(L, 1);
}
static int input_list_dump(lua_State* L)
{
  InputList* list = ilua::checkobject<InputList>(L, 1, "input.list");
  list->dump(L);
  return 1;
}
static int input_list_undump(lua_State* L)
{
  size_t size;
  char const* ptr = luaL_checklstring(L, 2, &size);
  InputList* list = ilua::checkobject<InputList>(L, 1, "input.list");
  list->undump(size, ptr);
  return 0;
}

static int input_newlist(lua_State* L)
{
  int top = lua_gettop(L);
  for (int i = 1; i <= top; i++)
    if (lua_type(L, i) != LUA_TNUMBER && lua_type(L, i) != LUA_TSTRING)
      luaL_argerror(L, i, "string or integer expected");

  InputModule* m = InputModule::get(L);
  InputList* list = new(L, "input.list") InputList;
  for (int i = 1; i <= top; i++)
  {
    if (lua_type(L, i) == LUA_TNUMBER)
      list->addkey(lua_tointeger(L, i));
    else
      list->addseq(lua_tostring(L, i), m->getkeymap());
  }
  return 1;
}
static int input_waitup(lua_State* L)
{
  int key;
  if (lua_type(L, 1) == LUA_TNUMBER)
    key = (lua_tointeger(L, 1) & 0xFF);
  else if (lua_type(L, 1) == LUA_TSTRING)
  {
    InputModule* m = InputModule::get(L);
    key = m->nametokey(lua_tostring(L, 1));
    lua_pushinteger(L, key);
    lua_replace(L, 1);
  }
  else
    luaL_argerror(L, 1, "string or integer expected");
  if (GetAsyncKeyState(key) & 0x8000)
    return ilua::engine(L)->current_thread()->yield(input_waitup);
  return 0;
}
static int input_waitdown(lua_State* L)
{
  int key;
  if (lua_type(L, 1) == LUA_TNUMBER)
    key = (lua_tointeger(L, 1) & 0xFF);
  else if (lua_type(L, 1) == LUA_TSTRING)
  {
    InputModule* m = InputModule::get(L);
    key = m->nametokey(lua_tostring(L, 1));
    lua_pushinteger(L, key);
    lua_replace(L, 1);
  }
  else
    luaL_argerror(L, 1, "string or integer expected");
  if (!(GetAsyncKeyState(key) & 0x8000))
    return ilua::engine(L)->current_thread()->yield(input_waitup);
  return 0;
}

extern "C" __declspec(dllexport) void StartModule(lua_State* L)
{
  ilua::newtype<InputModule>(L, MODULENAME, "object");
  lua_pop(L, 2);

  ilua::newtype<InputList>(L, "input.list");
  ilua::bindmethod(L, "addkeys", input_list_addkeys);
  ilua::bindmethod(L, "addtext", input_list_addtext);
  ilua::bindmethod(L, "addmove", input_list_addmove);
  ilua::bindmethod(L, "delay", input_list_delay);
  ilua::bindmethod(L, "randomize", input_list_randomize);
  ilua::bindmethod(L, "length", input_list_length);
  ilua::bindmethod(L, "duration", input_list_duration);
  ilua::bindmethod(L, "send", input_list_send);
  ilua::bindmethod(L, "recordstart", input_list_recordstart);
  ilua::bindmethod(L, "recordstop", input_list_recordstop);
  ilua::bindmethod(L, "dump", input_list_dump);
  ilua::bindmethod(L, "undump", input_list_undump);
  lua_pop(L, 2);

  ilua::openlib(L, "input");
  ilua::bindmethod(L, "hook", input_hook);
  ilua::bindmethod(L, "bind", input_bind);
  ilua::bindmethod(L, "send", input_send);
  ilua::bindmethod(L, "sendtext", input_sendtext);
  ilua::bindmethod(L, "ontext", input_ontext);
  ilua::bindmethod(L, "onsequence", input_onsequence);
  ilua::bindmethod(L, "newlist", input_newlist);
  lua_pop(L, 1);

  ilua::openlib(L, "key");
  lua_newtable(L);
  lua_newtable(L);
  ilua::bindmethod(L, "pressed", input_pressed);
  ilua::bindmethod(L, "waitup", input_waitup);
  ilua::bindmethod(L, "waitdown", input_waitdown);
  ilua::bindmethod(L, "name", key_name);
  lua_newtable(L);
  ilua::settabsi(L, "ALT", mALT);
  ilua::settabsi(L, "CONTROL", mCTRL);
  ilua::settabsi(L, "SHIFT", mSHIFT);
  ilua::settabsi(L, "WIN", mWIN);
  ilua::settabsi(L, "DOWN", mDOWN);
  ilua::settabsi(L, "UP", mUP);
  lua_setfield(L, -2, "mod");
  lua_setfield(L, -2, "__index");
  lua_setmetatable(L, -2);

  for (uint32 i = 0; i < 256; i++)
    if (keynames[i] && mousekeys[i] == 0)
      ilua::settabsi(L, keynames[i], i);
  lua_pop(L, 1);

  ilua::openlib(L, "mouse");
  lua_newtable(L);
  lua_newtable(L);
  ilua::bindmethod(L, "pressed", input_pressed);
  bind_mouse(L);
  lua_setfield(L, -2, "__index");
  lua_setmetatable(L, -2);

  for (uint32 i = 0; i < 256; i++)
    if (keynames[i] && mousekeys[i])
      ilua::settabsi(L, keynames[i], i);
  lua_pop(L, 1);

  new(L, MODULENAME) InputModule();
  lua_setfield(L, LUA_REGISTRYINDEX, MODULENAME);
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
  return TRUE;
}
