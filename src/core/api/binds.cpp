#include "engine.h"
#include "base/hashmap.h"
#include "core/ui/luaeval.h"
#include <time.h>

namespace api
{

static int global_include(lua_State* L)
{
  if (engine(L)->load_function(L, luaL_checkstring(L, 1)))
    return ilua::ycall(L, 0, LUA_MULTRET);
  else
    luaL_error(L, "failed to open include file '%s'", lua_tostring(L, 1));
  return 0;
}
static int global_load(lua_State* L)
{
  if (!engine(L)->load_function(L, luaL_checkstring(L, 1)))
    lua_pushnil(L);
  return 1;
}
static int global_require(lua_State* L)
{
  if (!engine(L)->load_module(luaL_checkstring(L, 1)))
    luaL_error(L, "failed to load module '%s'", lua_tostring(L, 1));
  return 0;
}
static int global_exit(lua_State* L)
{
  Engine* e = engine(L);
  e->exit(luaL_optinteger(L, 1, 0));
  return e->current_thread()->yield();
}

static int global_iskindof(lua_State* L)
{
  char const* t = luaL_optstring(L, 2, "object");
  if (ilua::iskindof(L, 1, t))
  {
    lua_getmetatable(L, 1);
    lua_rawgeti(L, -1, 0);
  }
  else
    lua_pushnil(L);
  return 1;
}

static int global_print(lua_State* L)
{
  int n = lua_gettop(L);
  String result = "";
  for (int i = 1; i <= n; i++)
  {
    size_t len;
    char const* str = luaL_tolstring(L, i, &len);
    if (i > 1)
      result += '\t';
    result += str;
    lua_pop(L, 1);
  }
  engine(L)->logMessage(result, LOG_OUTPUT);
  return 0;
}

static int global_sleep(lua_State* L)
{
  Thread* t = engine(L)->current_thread();
  return t->sleep(uint32(luaL_checknumber(L, 1) * 1000));
}
static int global_after(lua_State* L)
{
  Engine* e = engine(L);
  double delay = luaL_checknumber(L, 1);
  luaL_argcheck(L, lua_isfunction(L, 2), 2, "function expected");
  lua_pushvalue(L, 2);
  Thread* t = (Thread*) e->create_thread(0);
  ilua::pushobject(L, t);
  e->sleep(t, uint32(delay * 1000));
  return 1;
}

static int global_clock(lua_State* L)
{
  static union {
    uint64 time64;
    struct {
      uint32 timelow;
      uint32 timehigh;
    };
  } tdata = {0};
  uint32 cur = timeGetTime();
  if ((tdata.timelow & (~cur)) & 0x80000000)
    tdata.timehigh++;
  tdata.timelow = cur;
  lua_pushnumber(L, double(tdata.time64) * 0.001);
  return 1;
}

static int global_wipe(lua_State* L)
{
  luaL_argcheck(L, lua_istable(L, 1), 1, "table expected");
  lua_pushnil(L);
  while (lua_next(L, 1))
  {
    lua_pushvalue(L, -2);
    lua_pushnil(L);
    lua_rawset(L, 1);
    lua_pop(L, 1);
  }
  lua_settop(L, 1);
  return 1;
}

static int thread_create(lua_State* L)
{
  Engine* e = engine(L);
  if (lua_isstring(L, 1))
  {
    ((Engine*) e)->load_function(L, lua_tostring(L, 1));
    lua_replace(L, 1);
  }
  luaL_argcheck(L, lua_isfunction(L, 1), 1, "function expected");
  ilua::pushobject(L, e->create_thread(lua_gettop(L) - 1));
  return 1;
}
static int thread_current(lua_State* L)
{
  ilua::pushobject(L, engine(L)->current_thread());
  return 1;
}
static int thread_lock(lua_State* L)
{
  Engine* e = engine(L);
  if (e->current_thread())
    ((Thread*) e->current_thread())->locked++;
  return 0;
}
static int thread_unlock(lua_State* L)
{
  Engine* e = engine(L);
  if (e->current_thread())
  {
    Thread* t = (Thread*) e->current_thread();
    if (--t->locked == 0)
      return t->yield();
  }
  return 0;
}
static int thread_yield(lua_State* L)
{
  Engine* e = engine(L);
  if (e->current_thread())
    ((Thread*) e->current_thread())->yield();
  return 0;
}

static int thread_terminate(lua_State* L)
{
  Thread* t = ilua::checkobject<Thread>(L, 1, "thread");
  t->terminate();
  if (t == engine(L)->current_thread())
    return lua_yield(L, 0);
  else
    return 0;
}

static int thread_state(lua_State* L)
{
  Thread* t = ilua::checkobject<Thread>(L, 1, "thread");
  lua_pushboolean(L, t && lua_status(t->state()) != LUA_YIELD);
  return 1;
}

static int thread_suspend(lua_State* L)
{
  Thread* t = ilua::checkobject<Thread>(L, 1, "thread");
  t->suspend();
  if (t == engine(L)->current_thread())
    return lua_yield(L, 0);
  else
    return 0;
}
static int thread_resume(lua_State* L)
{
  Thread* t = ilua::checkobject<Thread>(L, 1, "thread");
  t->resume();
  return 0;
}

static int os_getenv(lua_State* L)
{
  char const* name = luaL_checkstring(L, 1);
  lua_pushstring(L, String(WideString::getenv(WideString(name))));
  return 1;
}
static int os_setenv(lua_State* L)
{
  char const* name = luaL_checkstring(L, 1);
  char const* value = luaL_optstring(L, 2, NULL);
  lua_pushboolean(L, SetEnvironmentVariable(WideString(name), value ? WideString(value).c_str() : NULL));
  return 1;
}
static int os_getenv2(lua_State* L)
{
  char const* name = luaL_checkstring(L, 2);
  lua_pushstring(L, String(WideString::getenv(WideString(name))));
  return 1;
}
static int os_setenv2(lua_State* L)
{
  char const* name = luaL_checkstring(L, 2);
  char const* value = luaL_optstring(L, 3, NULL);
  lua_pushboolean(L, SetEnvironmentVariable(WideString(name), value ? WideString(value).c_str() : NULL));
  return 1;
}

static int os_execfile(lua_State* L)
{
  char const* name = luaL_checkstring(L, 1);
  char const* cmd = luaL_optstring(L, 2, NULL);
  char const* args = luaL_optstring(L, 3, NULL);
  lua_pushboolean(L, (int) ShellExecute(NULL, cmd ? WideString(cmd).c_str() : NULL,
    WideString(name), args ? WideString(args).c_str() : NULL, NULL, SW_SHOWNORMAL) > 32);
  return 1;
}
struct ExecuteData
{
  Engine* engine;
  Thread* thread;
  HANDLE hProcess;
};
static DWORD WINAPI os_execute_proc(LPVOID arg)
{
  ExecuteData* data = (ExecuteData*) arg;
  WaitForSingleObject(data->hProcess, INFINITE);
  DWORD code = 0;
  GetExitCodeProcess(data->hProcess, &code);
  if (code == STILL_ACTIVE) code = 0;
  lua_State* L = data->engine->lock();
  lua_pushinteger(L, code);
  data->thread->resume(1);
  data->thread->release();
  data->engine->unlock();
  delete data;
  return 0;
}
static int os_execute(lua_State* L)
{
  size_t length = 0;
  char const* cmd = luaL_checklstring(L, 1, &length);
  int ulength = MultiByteToWideChar(CP_UTF8, 0, cmd, length + 1, NULL, 0);
  wchar_t* ucmd = new wchar_t[ulength + 11];
  wcscpy(ucmd, L"cmd.exe /c ");
  MultiByteToWideChar(CP_UTF8, 0, cmd, length + 1, ucmd + 11, ulength);
  STARTUPINFO info;
  GetStartupInfo(&info);
  info.dwFlags |= STARTF_USESHOWWINDOW;
  info.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION pi;
  if (CreateProcess(NULL, ucmd, NULL, NULL, FALSE, 0, NULL, NULL, &info, &pi))
  {
    delete[] ucmd;
    CloseHandle(pi.hThread);
    ExecuteData* data = new ExecuteData;
    data->engine = engine(L);
    data->thread = data->engine->current_thread();
    data->hProcess = pi.hProcess;
    data->thread->addref();
    data->thread->suspend();
    CreateThread(NULL, 0, os_execute_proc, data, 0, NULL);
    return lua_yield(L, 0);
  }
  delete[] ucmd;
  lua_pushboolean(L, 0);
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

static int time_time(lua_State* L)
{
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  lua_pushnumber(L, ft_to_time(ft));
  return 1;
}
static void pushtime(lua_State* L, SYSTEMTIME& st)
{
  lua_newtable(L);
  ilua::settabsi(L, "year", st.wYear);
  ilua::settabsi(L, "month", st.wMonth);
  ilua::settabsi(L, "day", st.wDay);
  ilua::settabsi(L, "wday", st.wDayOfWeek + 1);
  ilua::settabsi(L, "hour", st.wHour);
  ilua::settabsi(L, "min", st.wMinute);
  ilua::settabsi(L, "sec", st.wSecond);
  ilua::settabsi(L, "ms", st.wMilliseconds);
}
static int torange(int x, int a, int b)
{
  if (x < a) x = a;
  if (x > b) x = b;
  return x;
}
static void readtime(lua_State* L, int idx, SYSTEMTIME& st)
{
  st.wYear = torange(ilua::gettabsi(L, idx, "year"), 1601, 30827);
  st.wMonth = torange(ilua::gettabsi(L, idx, "month"), 1, 12);
  st.wDay = torange(ilua::gettabsi(L, idx, "day"), 1, 31);
  st.wDayOfWeek = torange(ilua::gettabsi(L, idx, "wday") - 1, 0, 6);
  st.wHour = torange(ilua::gettabsi(L, idx, "hour"), 0, 23);
  st.wMinute = torange(ilua::gettabsi(L, idx, "min"), 0, 59);
  st.wSecond = torange(ilua::gettabsi(L, idx, "sec"), 0, 59);
  st.wMilliseconds = torange(ilua::gettabsi(L, idx, "ms"), 0, 999);
}
static int time_tolocal(lua_State* L)
{
  SYSTEMTIME lt;
  if (lua_isnoneornil(L, 1))
    GetLocalTime(&lt);
  else
  {
    FILETIME ft;
    SYSTEMTIME st;
    time_to_ft(luaL_checknumber(L, 1), ft);
    FileTimeToSystemTime(&ft, &st);
    SystemTimeToTzSpecificLocalTime(NULL, &st, &lt);
  }
  pushtime(L, lt);
  return 1;
}
static int time_toutc(lua_State* L)
{
  SYSTEMTIME st;
  if (lua_isnoneornil(L, 1))
    GetSystemTime(&st);
  else
  {
    FILETIME ft;
    time_to_ft(luaL_checknumber(L, 1), ft);
    FileTimeToSystemTime(&ft, &st);
  }
  pushtime(L, st);
  return 1;
}
static int time_fromlocal(lua_State* L)
{
  luaL_argcheck(L, lua_istable(L, 1), 1, "table expected");
  SYSTEMTIME lt;
  SYSTEMTIME st;
  FILETIME ft;
  readtime(L, 1, lt);
  TzSpecificLocalTimeToSystemTime(NULL, &lt, &st);
  SystemTimeToFileTime(&st, &ft);
  lua_pushnumber(L, ft_to_time(ft));
  return 1;
}
static int time_fromutc(lua_State* L)
{
  luaL_argcheck(L, lua_istable(L, 1), 1, "table expected");
  SYSTEMTIME st;
  FILETIME ft;
  readtime(L, 1, st);
  SystemTimeToFileTime(&st, &ft);
  lua_pushnumber(L, ft_to_time(ft));
  return 1;
}
static int time_format(lua_State* L)
{
  char const* fmt = luaL_optstring(L, 1, "%c");
  SYSTEMTIME st;
  FILETIME ft;
  if (lua_isnoneornil(L, 2))
  {
    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
  }
  else if (lua_isnumber(L, 2))
    time_to_ft(lua_tonumber(L, 2), ft);
  else if (lua_istable(L, 2))
  {
    SYSTEMTIME lt;
    readtime(L, 2, lt);
    TzSpecificLocalTimeToSystemTime(NULL, &lt, &st);
    SystemTimeToFileTime(&st, &ft);
  }
  else
    luaL_argerror(L, 2, "number, table or nil expected");
  int64 t = int64(ft.dwLowDateTime) | (int64(ft.dwHighDateTime) << 32);
  t = t / 10000000ULL - 11644473600ULL;
  if (t < 0) t = 0;
  tm timedata;
  char buf[128];
  if (_localtime64_s(&timedata, (__time64_t*) &t) &&
      strftime(buf, sizeof buf, fmt, &timedata))
    lua_pushstring(L, buf);
  else
    lua_pushnil(L);
  return 1;
}
static int time_formatutc(lua_State* L)
{
  char const* fmt = luaL_optstring(L, 1, "%c");
  SYSTEMTIME st;
  FILETIME ft;
  if (lua_isnoneornil(L, 2))
  {
    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
  }
  else if (lua_isnumber(L, 2))
    time_to_ft(lua_tonumber(L, 2), ft);
  else if (lua_istable(L, 2))
  {
    readtime(L, 2, st);
    SystemTimeToFileTime(&st, &ft);
  }
  else
    luaL_argerror(L, 2, "number, table or nil expected");
  int64 t = int64(ft.dwLowDateTime) | (int64(ft.dwHighDateTime) << 32);
  t = t / 10000000ULL - 11644473600ULL;
  if (t < 0) t = 0;
  tm timedata;
  char buf[128];
  if (_gmtime64_s(&timedata, (__time64_t*) &t) &&
      strftime(buf, sizeof buf, fmt, &timedata))
    lua_pushstring(L, buf);
  else
    lua_pushnil(L);
  return 1;
}

static int string_hex(lua_State* L)
{
  static char hexl[] = "0123456789abcdef";
  static char hexu[] = "0123456789ABCDEF";
  size_t length;
  char const* str = luaL_checklstring(L, 1, &length);
  char const* conv = (lua_toboolean(L, 2) ? hexu : hexl);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for (size_t i = 0; i < length; i++)
  {
    luaL_addchar(&b, conv[(str[i] >> 4) & 0xF]);
    luaL_addchar(&b, conv[str[i] & 0xF]);
  }
  luaL_pushresult(&b);
  return 1;
}
static int string_pad(lua_State* L)
{
  size_t length;
  char const* str = luaL_checklstring(L, 1, &length);
  int padto = luaL_checkinteger(L, 2);

  if (length == padto)
    lua_pushvalue(L, 1);
  else if (length > padto)
    lua_pushlstring(L, str, padto);
  else
  {
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    luaL_addlstring(&b, str, length);
    char* ptr = luaL_prepbuffsize(&b, padto - length);
    memset(ptr, 0, padto - length);
    luaL_addsize(&b, padto - length);
    luaL_pushresult(&b);
  }
  return 1;
}

static bool isIdStr(char const* str, int len = -1)
{
  if (len < 0) len = strlen(str);
  if (len == 0) return false;
  if (s_isdigit(str[0])) return false;
  for (int p = 0; p < len; p++)
    if (!s_isalnum(str[p]) && str[p] != '_')
      return false;
  return true;
}
static String formatkey(lua_State* L, int idx)
{
  switch (lua_type(L, idx))
  {
  case LUA_TNONE:
  case LUA_TNIL:
    return "[nil]";
  case LUA_TBOOLEAN:
    return lua_toboolean(L, idx) ? "[true]" : "[false]";
  case LUA_TNUMBER:
    return String::format("[%.14g]", lua_tonumber(L, idx));
  case LUA_TSTRING:
    {
      size_t len;
      char const* str = lua_tolstring(L, idx, &len);
      if (isIdStr(str, len))
        return str;
      else
        return String::format("[\"%s\"]", luae::formatstr(str, len));
    }
    break;
  default:
    return String::format("[%s: %p]", lua_typename(L, lua_type(L, -1)), lua_topointer(L, -1));
  }
}
static void dump_append(lua_State* L, String& str, int pos, String prefix, int cache)
{
  pos = lua_absindex(L, pos);
  int type = lua_type(L, pos);
  switch (type)
  {
  case LUA_TNONE:
    return;
  case LUA_TNIL:
    str += "nil";
    return;
  case LUA_TBOOLEAN:
    str += (lua_toboolean(L, pos) ? "true" : "false");
    return;
  case LUA_TNUMBER:
    str.printf("%.14g", lua_tonumber(L, pos));
    return;
  case LUA_TSTRING:
    {
      size_t length;
      char const* text = lua_tolstring(L, pos, &length);
      str.printf("\"%s\"", luae::formatstr(text, length));
    }
    return;
  case LUA_TTABLE:
    {
      lua_pushvalue(L, pos);
      lua_rawget(L, cache);
      if (lua_isnil(L, -1))
      {
        lua_pop(L, 1);
        lua_pushvalue(L, pos);
        lua_pushboolean(L, 1);
        lua_rawset(L, cache);

        int len = lua_rawlen(L, pos);
        int before = 0;
        lua_pushnil(L);
        while (lua_next(L, pos))
        {
          if (lua_type(L, -2) == LUA_TNUMBER)
          {
            lua_Number n = lua_tonumber(L, -2);
            int k = int(n);
            if (lua_Number(k) == n && k >= 1 && k <= len)
              before++;
            else
            {
              lua_pop(L, 2);
              before = -1;
              break;
            }
          }
          else
          {
            lua_pop(L, 2);
            before = -1;
            break;
          }
          lua_pop(L, 1);
        }
        if (len == 0 && before == 0)
        {
          str += "{}";
          return;
        }
        str += "{\n";
        String nprefix = prefix + "  ";
        if (before == len)
        {
          for (int i = 1; i <= len; i++)
          {
            if (i > 1) str += ",\n";
            lua_rawgeti(L, pos, i);
            str += nprefix;
            dump_append(L, str, -1, nprefix, cache);
          }
        }
        else
        {
          lua_pushnil(L);
          bool first = true;
          while (lua_next(L, pos))
          {
            if (!first) str += ",\n";
            first = false;
            str += nprefix;
            str += formatkey(L, -2);
            str += " = ";
            dump_append(L, str, -1, nprefix, cache);
            lua_pop(L, 1);
          }
        }
        str.printf("\n%s}", prefix);
        return;
      }
      else
        lua_pop(L, 1);
    }
  default:
    if (type == LUA_TUSERDATA && lua_getmetatable(L, pos))
    {
      lua_rawgeti(L, -1, 0);
      if (char const* name = lua_tostring(L, -1))
        str.printf("%s: 0x%p", name, lua_topointer(L, pos));
      else
        str.printf("%s: 0x%p", lua_typename(L, type), lua_topointer(L, pos));
      lua_pop(L, 2);
    }
    else
      str.printf("%s: 0x%p", lua_typename(L, type), lua_topointer(L, pos));
    return;
  }
}
static int global_dump(lua_State* L)
{
  String result = "";
  int n = lua_gettop(L);
  for (int i = 1; i <= n; i++)
  {
    lua_newtable(L);
    dump_append(L, result, i, "", lua_gettop(L));
    if (i < n)
      result += "\n";
    lua_pop(L, 1);
  }
  engine(L)->logMessage(result, LOG_OUTPUT);
  return 0;
}

void Engine::bind(lua_State* L)
{
  lua_register(L, "include", global_include);
  lua_register(L, "load", global_load);
  lua_register(L, "require", global_require);
  lua_register(L, "iskindof", global_iskindof);
  lua_register(L, "sleep", global_sleep);
  lua_register(L, "print", global_print);
  lua_register(L, "exit", global_exit);
  lua_register(L, "clock", global_clock);
  lua_register(L, "after", global_after);
  lua_register(L, "wipe", global_wipe);
  lua_register(L, "dump", global_dump);

  ilua::openlib(L, "thread");
  ilua::bindmethod(L, "create", thread_create);
  ilua::bindmethod(L, "current", thread_current);
  ilua::bindmethod(L, "lock", thread_lock);
  ilua::bindmethod(L, "unlock", thread_unlock);
  ilua::bindmethod(L, "yield", thread_yield);
  lua_pop(L, 1);

  ilua::openlib(L, "time");
  ilua::bindmethod(L, "time", time_time);
  ilua::bindmethod(L, "tolocal", time_tolocal);
  ilua::bindmethod(L, "toutc", time_toutc);
  ilua::bindmethod(L, "fromlocal", time_fromlocal);
  ilua::bindmethod(L, "fromutc", time_fromutc);
  ilua::bindmethod(L, "format", time_format);
  ilua::bindmethod(L, "formatutc", time_formatutc);
  lua_pop(L, 1);

  ilua::openlib(L, "os");
  ilua::bindmethod(L, "execute", os_execute);
  ilua::bindmethod(L, "execfile", os_execfile);
  ilua::bindmethod(L, "getenv", os_getenv);
  ilua::bindmethod(L, "setenv", os_setenv);
  lua_newtable(L);
  lua_newtable(L);
  ilua::bindmethod(L, "__index", os_getenv2);
  ilua::bindmethod(L, "__newindex", os_setenv2);
  lua_setmetatable(L, -2);
  lua_setfield(L, -2, "environ");
  lua_pop(L, 1);

  ilua::newtype<Thread>(L, "thread", "object");
  ilua::bindmethod(L, "state", thread_state);
  ilua::bindmethod(L, "suspend", thread_suspend);
  ilua::bindmethod(L, "resume", thread_resume);
  ilua::bindmethod(L, "terminate", thread_terminate);
  lua_pop(L, 2);

  ilua::openlib(L, "string");
  ilua::bindmethod(L, "hex", string_hex);
  ilua::bindmethod(L, "pad", string_pad);
  lua_pop(L, 1);
}

}
