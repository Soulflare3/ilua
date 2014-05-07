#include "file.h"
#include "fileutil.h"
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <windows.h>
#include "base/thread.h"
#include "base/wstring.h"
#include "ilua/slowop.h"

class FileOp : public ilua::SlowOperation
{
  uint32 m_func;
  WideString m_from;
  WideString m_to;
public:
  enum {fMove = 1, fCopy = 2, fDelete = 3};
  FileOp(uint32 func, WideString from, WideString to)
    : m_func(func)
    , m_from(from)
    , m_to(to)
  {}

  void run()
  {
    SHFILEOPSTRUCT fo;
    memset(&fo, 0, sizeof fo);
    fo.wFunc = m_func;
    fo.pFrom = m_from.c_str();
    fo.pTo = m_to.c_str();
    fo.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_SILENT;
    if (fo.wFunc != FO_DELETE)
      fo.fFlags |= FOF_MULTIDESTFILES;
    int result = SHFileOperation(&fo);

    lua_State* L = output_begin();
    lua_pushboolean(L, result == 0);
    output_end(1);
  }
};

static int file_open(lua_State* L)
{
  char const* path = luaL_checkstring(L, 1);
  char const* mode = luaL_optstring(L, 2, "r");
  int mask = 0;
  for (int i = 0; mode[i]; i++)
  {
    switch (mode[i])
    {
    case 'r':
    case 'R':
      mask |= 1;
      break;
    case 'a':
    case 'A':
      mask |= 4;
    case 'w':
    case 'W':
      mask |= 2;
      break;
    }
  }
  DWORD creation[4] = {OPEN_EXISTING, OPEN_EXISTING, CREATE_ALWAYS, OPEN_ALWAYS};
  HANDLE hFile = CreateFile(WideString(path), GENERIC_READ | (mask & 2 ? GENERIC_WRITE : 0), FILE_SHARE_READ, NULL,
    creation[mask & 3], FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile != INVALID_HANDLE_VALUE)
    new(L, "file") SystemFile(hFile, mask & 4 ? SystemFile::OPEN_APPEND : (mask & 2 ? SystemFile::OPEN_WRITE : SystemFile::OPEN_READ));
  else
    lua_pushnil(L);
  return 1;
}
static int file_copystat(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  char const* arg2 = luaL_checkstring(L, 2);
  WideString src(arg1);
  WideString dst(arg2);
  WIN32_FILE_ATTRIBUTE_DATA attr;
  HANDLE hFile;
  if (!GetFileAttributesEx(src, GetFileExInfoStandard, &attr))
    lua_pushboolean(L, 0);
  else if (!SetFileAttributes(dst, attr.dwFileAttributes))
    lua_pushboolean(L, 0);
  else if ((hFile = CreateFile(dst, GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL)) == INVALID_HANDLE_VALUE)
    lua_pushboolean(L, 0);
  else
  {
    SetFileTime(hFile, &attr.ftCreationTime, &attr.ftLastAccessTime, &attr.ftLastWriteTime);
    CloseHandle(hFile);
    lua_pushboolean(L, 1);
  }
  return 1;
}
static int file_copy(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  char const* arg2 = luaL_checkstring(L, 2);
  WideString src = WideString::getFullPathName(WideString(arg1));
  WideString dst = WideString::getFullPathName(WideString(arg2));
  src.append(L"\0", 1);
  dst.append(L"\0", 1);
  FileOp* op = new FileOp(FileOp::fCopy, src, dst);
  op->start(L);
  return lua_yield(L, 0);
}

struct FindData
{
  WideString src;
  WideString dst;
  WIN32_FIND_DATA data;
  HANDLE hFind;
  BOOL success;
};
struct CopyTreeData
{
  int callback;
  int func;
  Array<FindData> stack;
  WideString from;
  WideString to;
  ~CopyTreeData()
  {
    for (int i = stack.length() - 1; i >= 0; i--)
      FindClose(stack[i].hFind);
  }
};

static int file_treefunc(lua_State* L)
{
  CopyTreeData* cd = ilua::tostruct<CopyTreeData>(L, 4);
  int accept = -1;
  if (lua_getctx(L, NULL) == LUA_YIELD)
  {
    accept = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }
  while (cd->stack.length())
  {
    if (cd->stack.top().success)
    {
      FindData& fd = cd->stack.top();
      if (wcscmp(fd.data.cFileName, L".") == 0 || wcscmp(fd.data.cFileName, L"..") == 0)
        accept = 0;
      if (accept < 0)
      {
        if (cd->callback > 0)
        {
          lua_pushvalue(L, cd->callback);
          lua_pushstring(L, String(fd.src));
          lua_pushstring(L, String(fd.data.cFileName));
          lua_pushboolean(L, fd.data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
          lua_callk(L, 3, 1, (int) cd, file_treefunc);
          accept = lua_toboolean(L, -1);
          lua_pop(L, 1);
        }
        else
          accept = 1;
      }
      if (accept)
      {
        WideString srcpath = WideString::buildFullName(fd.src, fd.data.cFileName);
        WideString dstpath = WideString::buildFullName(fd.dst, fd.data.cFileName);
        if (fd.data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
          CreateDirectory(dstpath, NULL);
          FindData& nfd = cd->stack.push();
          nfd.src = srcpath;
          nfd.dst = dstpath;
          nfd.hFind = INVALID_HANDLE_VALUE;
          nfd.success = TRUE;
        }
        else if (cd->func)
        {
          cd->from.append(srcpath, srcpath.length() + 1);
          cd->to.append(dstpath, dstpath.length() + 1);
        }
      }
      accept = -1;
    }
    else
    {
      FindData& fd = cd->stack.top();
      if (fd.hFind != INVALID_HANDLE_VALUE)
        FindClose(fd.hFind);
      cd->stack.pop();
    }
    if (cd->stack.length())
    {
      FindData& fd = cd->stack.top();
      if (fd.hFind == INVALID_HANDLE_VALUE)
      {
        fd.hFind = FindFirstFile(WideString::buildFullName(fd.src, L"*"), &fd.data);
        fd.success = (fd.hFind != INVALID_HANDLE_VALUE);
      }
      else
        fd.success = FindNextFile(fd.hFind, &fd.data);
    }
  }
  if (cd->func)
  {
    FileOp* op = new FileOp(cd->func, cd->from, cd->to);
    op->start(L);
    return lua_yield(L, 0);
  }
  else
    return 0;
}
static int os_copytree(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  char const* arg2 = luaL_checkstring(L, 2);
  if (lua_gettop(L) > 2)
    luaL_argcheck(L, lua_isfunction(L, 3), 3, "function expected");
  WideString src = WideString::getFullPathName(WideString(arg1));
  WideString dst = WideString::getFullPathName(WideString(arg2));
  if (!PathFileExists(src) ||
      (SHPathPrepareForWrite(NULL, NULL, dst, SHPPFW_DEFAULT) != S_OK))
  {
    lua_pushboolean(L, 0);
    return 1;
  }
  int callback = (lua_gettop(L) > 2 ? 3 : 0);
  lua_settop(L, 3);

  CopyTreeData* cd = ilua::newstruct<CopyTreeData>(L);
  FindData& fd = cd->stack.push();
  fd.src = src;
  fd.dst = dst;
  fd.hFind = FindFirstFile(WideString::buildFullName(fd.src, L"*"), &fd.data);
  fd.success = (fd.hFind != INVALID_HANDLE_VALUE);
  cd->callback = callback;
  cd->func = FileOp::fCopy;
  return file_treefunc(L);
}
static int os_rmtree(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  WideString path = WideString::getFullPathName(WideString(arg1));
  path.append(L"\0", 1);

  FileOp* op = new FileOp(FileOp::fDelete, path, WideString(L"\0", 1));
  op->start(L);
  return lua_yield(L, 0);
}
static int file_move(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  char const* arg2 = luaL_checkstring(L, 2);
  WideString src = WideString::getFullPathName(WideString(arg1));
  WideString dst = WideString::getFullPathName(WideString(arg2));
  src.append(L"\0", 1);
  dst.append(L"\0", 1);
  FileOp* op = new FileOp(FileOp::fMove, src, dst);
  op->start(L);
  return lua_yield(L, 0);
}
static int file_remove(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  lua_pushboolean(L, DeleteFile(WideString(arg1)));
  return 1;
}
static int os_mkdir(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  lua_pushboolean(L, CreateDirectory(WideString(arg1), NULL));
  return 1;
}
static int os_rmdir(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  lua_pushboolean(L, RemoveDirectory(WideString(arg1)));
  return 1;
}
static int os_makedirs(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  WideString path = WideString::getFullPathName(WideString(arg1));
  lua_pushboolean(L, SHPathPrepareForWrite(NULL, NULL, path, SHPPFW_DEFAULT) == S_OK);
  return 1;
}
static int file_temp(lua_State* L)
{
  wchar_t pbuf[MAX_PATH + 10];
  wchar_t fbuf[MAX_PATH + 10];
  GetTempPath(MAX_PATH + 10, pbuf);
  if (GetTempFileName(pbuf, L"lua", 0, fbuf))
  {
    HANDLE hFile = CreateFile(fbuf, GENERIC_READ | GENERIC_WRITE,
      0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
      new(L, "file") SystemFile(hFile, SystemFile::OPEN_WRITE);
    else
      lua_pushnil(L);
  }
  else
    lua_pushnil(L);
  return 1;
}

struct ListDirData
{
  WIN32_FIND_DATA data;
  HANDLE hFind;
  ~ListDirData()
  {
    FindClose(hFind);
  }
};
static int os_listdir_iter(lua_State* L)
{
  ListDirData* ld = ilua::tostruct<ListDirData>(L, 1);
  BOOL success = (ld->hFind != INVALID_HANDLE_VALUE);
  if (success && !lua_isnoneornil(L, 2))
    success = FindNextFile(ld->hFind, &ld->data);
  while (success && (!wcscmp(ld->data.cFileName, L".") || !wcscmp(ld->data.cFileName, L"..")))
    success = FindNextFile(ld->hFind, &ld->data);
  if (success)
  {
    lua_pushstring(L, String(ld->data.cFileName));
    lua_pushboolean(L, ld->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    return 2;
  }
  if (ld->hFind != INVALID_HANDLE_VALUE)
  {
    FindClose(ld->hFind);
    ld->hFind = INVALID_HANDLE_VALUE;
  }
  lua_pushnil(L);
  return 1;
}
static int os_listdir(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  WideString path = WideString::getFullPathName(WideString(arg1));
  lua_pushcfunction(L, os_listdir_iter);
  ListDirData* ld = ilua::newstruct<ListDirData>(L);
  ld->hFind = FindFirstFile(WideString::buildFullName(path, L"*"), &ld->data);
  lua_pushnil(L);
  return 3;
}

struct WalkData
{
  struct Level
  {
    WideString path;
    int pos;
  };
  Array<Level> stack;
};
static void os_walk_list(lua_State* L, WideString path)
{
  lua_newtable(L);
  lua_newtable(L);
  WIN32_FIND_DATA data;
  HANDLE hFind = FindFirstFile(WideString::buildFullName(path, L"*"), &data);
  BOOL success = (hFind != INVALID_HANDLE_VALUE);
  while (success)
  {
    if (wcscmp(data.cFileName, L".") && wcscmp(data.cFileName, L".."))
    {
      lua_pushstring(L, String(data.cFileName));
      int pos = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? -3 : -2);
      int len = lua_rawlen(L, pos);
      lua_rawseti(L, pos, len + 1);
    }
    success = FindNextFile(hFind, &data);
  }
  if (hFind != INVALID_HANDLE_VALUE)
    FindClose(hFind);
}
static int os_walk_fwditer(lua_State* L)
{
  lua_rawgeti(L, 1, 0);
  WalkData* wd = ilua::tostruct<WalkData>(L, -1);
  lua_pop(L, 1);
  WalkData::Level* lv = &wd->stack.top();
  if (lv->pos < 0)
  {
    lv->pos = 0;
    lua_pushstring(L, String(lv->path));
    os_walk_list(L, lv->path);
    lua_pushvalue(L, -2);
    lua_rawseti(L, 1, wd->stack.length());
    return 3;
  }
  while (wd->stack.length())
  {
    lv = &wd->stack.top();
    lv->pos++;
    lua_rawgeti(L, 1, wd->stack.length());
    lua_rawgeti(L, -1, lv->pos);
    char const* cpath = lua_tostring(L, -1);
    if (cpath)
    {
      WideString path = WideString::buildFullName(lv->path, WideString(cpath));
      lv = &wd->stack.push();
      lv->path = path;
      lv->pos = 0;
      lua_pushstring(L, String(path));
      os_walk_list(L, path);
      lua_pushvalue(L, -2);
      lua_rawseti(L, 1, wd->stack.length());
      return 3;
    }
    lua_pop(L, 2);
    lua_pushnil(L);
    lua_rawseti(L, 1, wd->stack.length());
    wd->stack.pop();
  }
  lua_pushnil(L);
  return 1;
}
static int os_walk_reviter(lua_State* L)
{
  lua_rawgeti(L, 1, 0);
  WalkData* wd = ilua::tostruct<WalkData>(L, -1);
  lua_pop(L, 1);
  if (wd->stack.length() == 0)
  {
    lua_pushnil(L);
    return 1;
  }
  WalkData::Level* lv = &wd->stack.top();
  if (lv->pos < 0)
  {
    lv->pos = 0;
    os_walk_list(L, lv->path);
    lua_rawseti(L, 1, -wd->stack.length());
    lua_rawseti(L, 1, wd->stack.length());
  }
  while (true)
  {
    lv->pos++;
    lua_rawgeti(L, 1, wd->stack.length());
    lua_rawgeti(L, -1, lv->pos);
    char const* cpath = lua_tostring(L, -1);
    if (cpath == NULL)
    {
      lua_pushstring(L, String(lv->path));
      lua_rawgeti(L, 1, wd->stack.length());
      lua_rawgeti(L, 1, -wd->stack.length());
      lua_pushnil(L);
      lua_pushnil(L);
      lua_rawseti(L, 1, -wd->stack.length());
      lua_rawseti(L, 1, wd->stack.length());
      wd->stack.pop();
      return 3;
    }
    WideString path = WideString::buildFullName(lv->path, WideString(cpath));
    lua_pop(L, 2);
    lv = &wd->stack.push();
    lv->path = path;
    lv->pos = 0;
    os_walk_list(L, path);
    lua_rawseti(L, 1, -wd->stack.length());
    lua_rawseti(L, 1, wd->stack.length());
  }
}
static int os_walk(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  WideString path = WideString::getFullPathName(WideString(arg1));
  if (lua_toboolean(L, 2))
    lua_pushcfunction(L, os_walk_reviter);
  else
    lua_pushcfunction(L, os_walk_fwditer);
  lua_newtable(L);
  WalkData* wd = ilua::newstruct<WalkData>(L);
  WalkData::Level& lv = wd->stack.push();
  lv.path = path;
  lv.pos = -1;
  lua_rawseti(L, -2, 0);
  lua_pushnil(L);
  return 3;
}
static int os_chdir(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  WideString path = WideString::getFullPathName(WideString(arg1));
  lua_pushboolean(L, SetCurrentDirectory(path));
  return 1;
}
static int os_getcwd(lua_State* L)
{
  lua_pushstring(L, String(WideString::getCurrentDir()));
  return 1;
}

static double ft_to_time(FILETIME const& ft)
{
  int64 t = int64(ft.dwLowDateTime) | (int64(ft.dwHighDateTime) << 32);
  return double(t) * 1e-7;
}
static void time_to_ft(double dt, FILETIME& ft)
{
  int64 t = int64(dt * 1e+7);
  ft.dwLowDateTime = uint32(t);
  ft.dwHighDateTime = uint32(t >> 32);
}
static FILETIME* opt_ft(lua_State* L, int idx, FILETIME& ft)
{
  if (lua_isnoneornil(L, idx))
    return NULL;
  time_to_ft(luaL_checknumber(L, idx), ft);
  return &ft;
}

static int file_stat(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  WIN32_FILE_ATTRIBUTE_DATA attr;
  if (!GetFileAttributesEx(WideString(arg1), GetFileExInfoStandard, &attr))
  {
    lua_pushnil(L);
    return 1;
  }
  else
  {
    lua_pushinteger(L, attr.dwFileAttributes);
    lua_pushnumber(L, double(int64(attr.nFileSizeLow) | (int64(attr.nFileSizeHigh) << 32)));
    lua_pushnumber(L, ft_to_time(attr.ftCreationTime));
    lua_pushnumber(L, ft_to_time(attr.ftLastAccessTime));
    lua_pushnumber(L, ft_to_time(attr.ftLastWriteTime));
    return 5;
  }
}
static int file_attr(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  DWORD attr = GetFileAttributes(WideString(arg1));
  if (attr == INVALID_FILE_ATTRIBUTES)
    lua_pushnil(L);
  else
    lua_pushinteger(L, attr);
  return 0;
}
static int file_time(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  WIN32_FILE_ATTRIBUTE_DATA attr;
  if (!GetFileAttributesEx(WideString(arg1), GetFileExInfoStandard, &attr))
  {
    lua_pushnil(L);
    return 1;
  }
  else
  {
    lua_pushnumber(L, ft_to_time(attr.ftCreationTime));
    lua_pushnumber(L, ft_to_time(attr.ftLastAccessTime));
    lua_pushnumber(L, ft_to_time(attr.ftLastWriteTime));
    return 3;
  }
}
static int file_setattr(lua_State* L)
{
  char const* arg1 = luaL_checkstring(L, 1);
  int attr = (luaL_checkinteger(L, 2) & 0x31A7);
  lua_pushboolean(L, SetFileAttributes(WideString(arg1), attr));
  return 1;
}
static int file_settime(lua_State* L)
{
  char const* name = luaL_checkstring(L, 1);
  HANDLE hFile = CreateFile(WideString(name), GENERIC_READ | GENERIC_WRITE,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (hFile != INVALID_HANDLE_VALUE)
  {
    FILETIME ft[3];
    lua_pushboolean(L, SetFileTime(hFile, opt_ft(L, 2, ft[0]),
      opt_ft(L, 3, ft[1]), opt_ft(L, 4, ft[2])));
    CloseHandle(hFile);
  }
  else
    lua_pushboolean(L, 0);
  return 1;
}

void fileutil_bind(lua_State* L)
{
  ilua::openlib(L, "file");
  ilua::bindmethod(L, "open", file_open);
  ilua::bindmethod(L, "copy", file_copy);
  ilua::bindmethod(L, "copystat", file_copystat);
  ilua::bindmethod(L, "move", file_move);
  ilua::bindmethod(L, "remove", file_remove);
  ilua::bindmethod(L, "temp", file_temp);
  ilua::bindmethod(L, "stat", file_stat);
  ilua::bindmethod(L, "attr", file_attr);
  ilua::bindmethod(L, "time", file_time);
  ilua::bindmethod(L, "setattr", file_setattr);
  ilua::bindmethod(L, "settime", file_settime);

  ilua::settabsi(L, "READONLY", FILE_ATTRIBUTE_READONLY);
  ilua::settabsi(L, "HIDDEN", FILE_ATTRIBUTE_HIDDEN);
  ilua::settabsi(L, "SYSTEM", FILE_ATTRIBUTE_SYSTEM);
  ilua::settabsi(L, "DIRECTORY", FILE_ATTRIBUTE_DIRECTORY);
  ilua::settabsi(L, "ARCHIVE", FILE_ATTRIBUTE_ARCHIVE);
  ilua::settabsi(L, "DEVICE", FILE_ATTRIBUTE_DEVICE);
  ilua::settabsi(L, "NORMAL", FILE_ATTRIBUTE_NORMAL);
  ilua::settabsi(L, "TEMPORARY", FILE_ATTRIBUTE_TEMPORARY);
  ilua::settabsi(L, "SPARSE_FILE", FILE_ATTRIBUTE_SPARSE_FILE);
  ilua::settabsi(L, "REPARSE_POINT", FILE_ATTRIBUTE_REPARSE_POINT);
  ilua::settabsi(L, "COMPRESSED", FILE_ATTRIBUTE_COMPRESSED);
  ilua::settabsi(L, "OFFLINE", FILE_ATTRIBUTE_OFFLINE);
  ilua::settabsi(L, "NOT_INDEXED", FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
  ilua::settabsi(L, "ENCRYPTED", FILE_ATTRIBUTE_ENCRYPTED);

  ilua::settabsi(L, "SEEK_SET", 0);
  ilua::settabsi(L, "SEEK_CUR", 1);
  ilua::settabsi(L, "SEEK_END", 2);

  lua_pop(L, 1);

  ilua::openlib(L, "os");
  ilua::bindmethod(L, "mkdir", os_mkdir);
  ilua::bindmethod(L, "rmdir", os_rmdir);
  ilua::bindmethod(L, "makedirs", os_makedirs);
  ilua::bindmethod(L, "copytree", os_copytree);
  ilua::bindmethod(L, "rmtree", os_rmtree);
  ilua::bindmethod(L, "listdir", os_listdir);
  ilua::bindmethod(L, "walk", os_walk);
  ilua::bindmethod(L, "chdir", os_chdir);
  ilua::bindmethod(L, "getcwd", os_getcwd);
  lua_pop(L, 1);

  path_bind(L);
}
