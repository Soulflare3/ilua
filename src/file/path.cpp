#include "fileutil.h"
#include "base/wstring.h"
#include "ilua/ilua.h"
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>

static int path_abspath(lua_State* L)
{
  char const* path = luaL_checkstring(L, 1);
  lua_pushstring(L, String(WideString::getFullPathName(WideString(path))));
  return 1;
}
static int path_basename(lua_State* L)
{
  char const* path = luaL_checkstring(L, 1);
  lua_pushstring(L, String::getFileName(path));
  return 1;
}
static int path_dirname(lua_State* L)
{
  char const* path = luaL_checkstring(L, 1);
  lua_pushstring(L, String::getPath(path));
  return 1;
}
static int path_exists(lua_State* L)
{
  char const* path = luaL_checkstring(L, 1);
  lua_pushboolean(L, PathFileExists(WideString::getFullPathName(WideString(path))));
  return 1;
}
static int path_expandvars(lua_State* L)
{
  char const* path = luaL_checkstring(L, 1);
  lua_pushstring(L, String(WideString::expandvars(WideString(path))));
  return 0;
}
static bool isabspath(char const* path)
{
  if (s_isalpha(path[0]) && path[1] == ':')
    path += 2;
  return (path[0] == '/' || path[0] == '\\');
}
static int path_isabs(lua_State* L)
{
  lua_pushboolean(L, isabspath(luaL_checkstring(L, 1)));
  return 1;
}
static int path_isfile(lua_State* L)
{
  char const* path = luaL_checkstring(L, 1);
  DWORD attr = GetFileAttributes(WideString(path));
  lua_pushboolean(L, attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
  return 1;
}
static int path_isdir(lua_State* L)
{
  char const* path = luaL_checkstring(L, 1);
  DWORD attr = GetFileAttributes(WideString(path));
  lua_pushboolean(L, attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
  return 1;
}

static int path_join(lua_State* L)
{
  int n = lua_gettop(L);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  char last_char = 0;
  for (int i = 1; i <= n; i++)
  {
    size_t length;
    char const* part = luaL_checklstring(L, i, &length);
    if (isabspath(part) && i > 1)
    {
      luaL_pushresult(&b);
      lua_pop(L, 1);
      luaL_buffinit(L, &b);
      last_char = 0;
    }
    if (last_char && last_char != '/' && last_char != '\\' && last_char != ':')
      luaL_addchar(&b, '\\');
    luaL_addlstring(&b, part, length);
    last_char = (length ? part[length - 1] : 0);
  }
  luaL_pushresult(&b);
  return 1;
}
static int path_normcase(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  lua_pushstring(L, String(WideString::normcase(WideString(arg1))));
  return 0;
}
static int path_normpath(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  lua_pushstring(L, String(WideString::normpath(WideString(arg1))));
  return 0;
}
static int path_relpath(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  char const* arg2 = luaL_optstring(L, 2, NULL);
  lua_pushstring(L, String(WideString::relpath(WideString(arg1),
    arg2 ? WideString(arg2) : WideString::getCurrentDir())));
  return 1;
}
static int path_split(lua_State* L)
{
  size_t length;
  char const* path = luaL_checklstring(L, 1, &length);
  size_t split = length;
  while (split > 0 && path[split - 1] != '\\' && path[split - 1] != '/')
    split--;
  size_t root = split;
  while (root > 0 && (path[root - 1] == '\\' || path[root - 1] == '/'))
    root--;
  size_t head = split;
  if (root != 0 && (root != 2 || s_isalpha(path[0]) || path[1] == ':'))
    head--;
  lua_pushlstring(L, path, head);
  lua_pushlstring(L, path + split, length - split);
  return 2;
}
static int path_splitext(lua_State* L)
{
  size_t length;
  char const* path = luaL_checklstring(L, 1, &length);
  size_t split = length;
  while (split > 0 && path[split - 1] != '\\' && path[split - 1] != '/' && path[split] != '.')
    split--;
  if (split == 0 || path[split - 1] == '\\' || path[split - 1] == '/')
    split = length;
  lua_pushlstring(L, path, split);
  if (split < length)
    lua_pushlstring(L, path + split, length - split);
  else
    lua_pushnil(L);
  return 2;
}

void path_bind(lua_State* L)
{
  ilua::openlib(L, "path");
  ilua::bindmethod(L, "abspath", path_abspath);
  ilua::bindmethod(L, "basename", path_basename);
  ilua::bindmethod(L, "dirname", path_dirname);
  ilua::bindmethod(L, "exists", path_exists);
  ilua::bindmethod(L, "expandvars", path_expandvars);
  ilua::bindmethod(L, "isabs", path_isabs);
  ilua::bindmethod(L, "isfile", path_isfile);
  ilua::bindmethod(L, "isdir", path_isdir);
  ilua::bindmethod(L, "join", path_join);
  ilua::bindmethod(L, "normcase", path_normcase);
  ilua::bindmethod(L, "normpath", path_normpath);
  ilua::bindmethod(L, "relpath", path_relpath);
  ilua::bindmethod(L, "split", path_split);
  ilua::bindmethod(L, "splitext", path_splitext);
  ilua::settabss(L, "sep", "\\");
  lua_pop(L, 1);
}
