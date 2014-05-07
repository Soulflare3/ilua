#include "engine.h"
#include <windows.h>
#include <stdio.h>
#include "sync.h"
#include "event.h"
#include "strlib.h"
#include "base/utils.h"

namespace api
{

void bind_sync(lua_State* L);
void bind_utf8(lua_State* L);
void bind_re(lua_State* L);
void bind_stream(lua_State* L);

// Thread

#define ILUA_TABLE_COUNTER      "ilua_threadcounter"

Thread::Thread(int id, int _narg)
  : narg(_narg)
  , paused(0)
  , locked(0)
  , waketime(0)
  , next(NULL)
  , prev(NULL)
  , tid(id)
  , dataptr(NULL)
  , tcounter(0)
  , epoch(0)
  , name("Worker thread")
  , tstatus(0)
{
  addref();

  lua_State* cL = e->lock();

  lua_pop(cL, 1);

  L = lua_newthread(cL);
  lua_pop(cL, 1);

  lua_xmove(cL, L, narg + 1);

  ((Engine*) e)->enqueue(this);

  e->unlock();
}

void Thread::suspend()
{
  e->lock();
  if (paused++ == 0)
    ((Engine*) e)->suspend(this);
  e->unlock();
}
void Thread::terminate()
{
  e->lock();
  ((Engine*) e)->terminate(this);
  e->unlock();
}
int Thread::yield(lua_CFunction cont, int ctx)
{
  ((Engine*) e)->enqueue(this);
  return lua_yieldk(L, 0, ctx, cont);
}
int Thread::sleep(int time, lua_CFunction cont, int ctx)
{
  ((Engine*) e)->sleep(this, time);
  return lua_yieldk(L, 0, ctx, cont);
}
void Thread::resume(Object* rc)
{
  e->lock();
  if (--paused == 0 && ((Engine*) e)->suspended(this))
    ((Engine*) e)->enqueue(this);

  if (rc)
  {
    ilua::pushobject(L, rc);
    narg = 1;
  }

  e->unlock();
}
void Thread::resume(int args)
{
  e->lock();
  if (--paused == 0 && ((Engine*) e)->suspended(this))
    ((Engine*) e)->enqueue(this);

  narg = args;

  e->unlock();
}
int Thread::run()
{
  int na = narg;
  narg = 0;
  locked = 0;
  return lua_resume(L, NULL, na);
}

Engine::Engine()
  : modules(DictionaryMap::asciiNoCase)
  , handler(NULL)
  , bpHandler(NULL)
  , bpOpaque(NULL)
  , hasBreakpoints(false)
  , hThread(NULL)
  , hasKeepalive(false)
  , L(NULL)
{
  resetvar();
}
Engine::~Engine()
{
  if (running())
    halt();
  if (hThread)
  {
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
  }
}

struct FileReaderData
{
  File* file;
  uint8 buf[1024];
};
static const char* FileReader(lua_State* L, void* data, size_t* size)
{
  FileReaderData* fr = (FileReaderData*) data;
  *size = fr->file->read(fr->buf, sizeof fr->buf);
  return (char*) fr->buf;
}
bool Engine::load_function(lua_State* cL, char const* path)
{
  WideString fullPath = WideString::getFullPathName(WideString(path));
  File* file = File::wopen(fullPath.c_str());
  String luaPath = "@" + String(fullPath);
  if (file)
  {
    FileReaderData* data = new FileReaderData;
    data->file = file;
    int code = lua_load(cL, FileReader, data, luaPath, NULL);
    delete data;

    if (handler)
    {
      LoadedInfo* info = new LoadedInfo;
      info->path = fullPath;
      //if (code == LUA_OK)
      //{
      //  lua_pushvalue(cL, -1);
      //  lua_Debug D;
      //  lua_getinfo(cL, ">L", &D);
      //  int ti = lua_gettop(cL);
      //  lua_pushnil(cL);
      //  while (lua_next(cL, ti) != 0)
      //  {
      //    int line = lua_tointeger(cL, -2);
      //    if (line > 0)
      //      info->validLines.push(line - 1);
      //    lua_pop(cL, 1);
      //  }
      //  lua_pop(cL, 1);
      //}
      SendMessage(handler, WM_OPENFILE, (WPARAM) info, (LPARAM) this);
      delete info;
    }

    if (code != LUA_OK)
    {
      char const* msg = lua_tostring(cL, -1);
      logMessage(msg, LOG_ERROR);
      if (bpHandler)
      {
        int pos = 0;
        while (msg[pos] && (msg[pos] != ':' || msg[pos + 1] < '0' || msg[pos + 1] > '9'))
          pos++;
        if (msg[pos])
          bpHandler(DBG_LOADERROR, atoi(msg + pos + 1) - 1, luaPath.c_str() + 1, bpOpaque);
      }
      lua_pop(cL, 1);
    }

    return (code == LUA_OK);
  }
  return false;
}
bool Engine::load(char const* path)
{
  bool wasRunning = running();
  if (!running())
    start();
  lua_State* cL = lock();
  bool success = load_function(cL, path);
  if (success)
    create_thread(0)->name = String::getFileName(path);
  else if (!wasRunning)
    finish();
  unlock();
  return success;
}

lua_State* Engine::lock()
{
  lockRequest.increment();
  luaLock.acquire();
  return cur_state();
}
void Engine::unlock()
{
  luaLock.release();
  lockRequest.decrement();
}

int Engine::get_counter(int id)
{
  lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_COUNTER);
  lua_rawgeti(L, -1, id);
  int res = (lua_isnumber(L, -1) ? lua_tointeger(L, -1) : 0);
  lua_pop(L, 2);
  return res;
}
void Engine::set_counter(int id, int value)
{
  lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_COUNTER);
  lua_pushinteger(L, value);
  lua_rawseti(L, -2, id);
  lua_rawgeti(L, -1, 0);
  lua_pushinteger(L, 1);
  lua_arith(L, LUA_OPADD);
  lua_pushvalue(L, -1);
  lua_rawseti(L, -3, 0);
  lua_rawseti(L, -2, -id);
  lua_pop(L, 1);
}
int Engine::add_counter(int id, int epoch, int value)
{
  lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_COUNTER);
  lua_rawgeti(L, -1, -id);
  int e = (lua_isnumber(L, -1) ? lua_tointeger(L, -1) : 0);
  lua_pop(L, 1);
  if (epoch == 0 || e == epoch)
  {
    lua_rawgeti(L, -1, id);
    int v = (lua_isnumber(L, -1) ? lua_tointeger(L, -1) : 0) + value;
    lua_pop(L, 1);
    lua_pushinteger(L, v);
    lua_rawseti(L, -2, id);
  }
  lua_pop(L, 1);
  return e;
}
Thread* Engine::create_thread(int narg, int counter)
{
  lua_State* cL = lock();
  Thread* thread = new(cL, "thread") Thread(++thread_count, narg);
  thread->tcounter = counter;
  if (counter)
    thread->epoch = add_counter(counter, 0, -1);
  unlock();
  if (hThread == NULL)
    hThread = thread::create(this, &Engine::core_thread);
  return thread;
}
void Engine::resetvar()
{
  nonEmptyQueue.reset();
  hasKeepalive = false;
  shutdown = false;
  exitCode = 0;
  first_thread = last_thread = NULL;
  sleep_first = sleep_last = NULL;
  suspend_first = suspend_last = NULL;
  cur_thread = NULL;
  cur_counter = 0;
  bpRun.set();
  dbgRequest = DBG_RUN;
  dbgDepth = 0;
  dbgThread = NULL;
  thread_count = 0;
}
void Engine::start()
{
  L = luaL_newstate();

  luaL_requiref(L, "_G", luaopen_base, 1);
  lua_pushnil(L);
  lua_setfield(L, -2, "dofile");
  lua_pop(L, 1);

  luaL_requiref(L, "table", luaopen_table, 1);
  lua_pop(L, 1);
  luaL_requiref(L, "string", luaopen_string, 1);
  lua_pop(L, 1);
  luaL_requiref(L, "bit", luaopen_bit32, 1);
  lua_pop(L, 1);
  luaL_requiref(L, "math", luaopen_math, 1);
  lua_pop(L, 1);

  lua_newtable(L);
  lua_pushinteger(L, 0);
  lua_rawseti(L, -2, 0);
  lua_setfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_COUNTER);

  lua_newtable(L);
  lua_setfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_META);
  lua_newtable(L);
  lua_setfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_BASIC);
  lua_newtable(L);
  lua_newtable(L);
  lua_pushstring(L, "v");
  lua_setfield(L, -2, "__mode");
  lua_setmetatable(L, -2);
  lua_setfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_BIND);
  lua_newtable(L);
  lua_setfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_XREF);
  lua_newtable(L);
  lua_newtable(L);
  lua_pushstring(L, "k");
  lua_setfield(L, -2, "__mode");
  lua_setmetatable(L, -2);
  lua_setfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_EXTRA);

  ilua::newtype<ilua::Object>(L, "object");
  lua_pop(L, 2);

  bind(L);
  bind_sync(L);
  bind_utf8(L);
  bind_re(L);
  bind_stream(L);

  lua_pushlightuserdata(L, this);
  lua_setfield(L, LUA_REGISTRYINDEX, "ilua_engine");
  sethook(L);

  if (hThread)
  {
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
  }
  hThread = NULL;
}
void Engine::finish()
{
  lua_close(L);
  L = NULL;

  if (handler)
    PostMessage(handler, WM_ENGINEHALT, 0, (LPARAM) this);

  for (uint32 cur = modules.enumStart(); cur; cur = modules.enumNext(cur))
    FreeModule(modules.enumGetValue(cur));
  modules.clear();

  resetvar();
}
void Engine::halt()
{
  if (hThread)
  {
    exitCode = 0;
    shutdown = true;
    nonEmptyQueue.set();
    bpRun.set();
  }
  else
    finish();
}

int Engine::core_thread()
{
  logMessage("Engine started", LOG_SYSTEM);
  if (handler)
    PostMessage(handler, WM_ENGINESTART, 0, (LPARAM) this);

  while ((!shutdown || suspend_first) && (first_thread || sleep_first || suspend_first || hasKeepalive))
  {
    if (shutdown)
    {
      Thread* cur = suspend_first;
      while (cur)
      {
        Thread* next = cur->next;
        if (cur->refcount() <= 1)
          enqueue(cur);
        cur = next;
      }
    }
    nonEmptyQueue.wait();
    luaLock.acquire();
    while ((first_thread != NULL || sleep_first != NULL) && !shutdown)
    {
      if (lockRequest)
      {
        luaLock.release();
        lockRequest.wait();
        luaLock.acquire();
      }

      cur_thread = first_thread;
      if (sleep_first && sleep_first->waketime < GetTickCount())
        cur_thread = sleep_first;
      if (cur_thread)
      {
        cur_counter = 1;
        sethook(cur_thread->state());
        dbgDepth = -1;
        int result = cur_thread->run();
        if (result != LUA_YIELD)
        {
          cur_thread->tstatus = 1;
          if (result != LUA_OK)
          {
            lua_State* cL = cur_thread->state();
            logMessage(lua_tostring(cL, -1), LOG_ERROR);
            lua_Debug D;
            int stackPos = 0;
            while (lua_getstack(cL, stackPos, &D) || stackPos > 100)
            {
              lua_getinfo(cL, "l", &D);
              if (D.currentline > 0)
                break;
              stackPos++;
            }
            lua_getinfo(cL, "Sl", &D);
            if (bpHandler(DBG_ERROR, D.currentline - 1, D.source + 1, bpOpaque))
              dbgBreak(cL, DBG_ERROR);
          }
          if (cur_thread->tcounter)
          {
            add_counter(cur_thread->tcounter, cur_thread->epoch, 1);
            cur_thread->tcounter = 0;
          }
          dequeue(cur_thread);
          cur_thread->release();
        }
        cur_thread = NULL;
      }
    }
    nonEmptyQueue.reset();
    luaLock.release();
  }
  logMessage(String::format("Engine stopped (exit code %d)", exitCode), LOG_SYSTEM);
  finish();
  return 0;
}
void Engine::hook_func(lua_State* L, lua_Debug* D)
{
  Engine* e = engine(L);
  if (e->lockRequest)
  {
    e->luaLock.release();
    e->lockRequest.wait();
    e->luaLock.acquire();
  }

  if ((D->event == LUA_HOOKCOUNT && e->cur_thread && (e->dbgRequest <= DBG_BREAK ||
    e->cur_thread != e->dbgThread)) || e->shutdown)
  {
    if (e->cur_counter > 0)
      e->cur_counter--;
    if ((e->cur_counter <= 0 && !e->cur_thread->locked) || e->shutdown)
    {
      e->cur_thread->yield();
      return;
    }
  }
  if (D->event == LUA_HOOKLINE && e->cur_thread && e->bpHandler)
  {
    if (((e->dbgRequest == DBG_STEP && e->dbgDepth <= 0) || e->dbgRequest == DBG_STEPIN ||
        (e->dbgRequest == DBG_STEPOUT && e->dbgDepth < 0)) &&
        (e->dbgThread == NULL || e->dbgThread == e->current_thread()))
    {
      lua_getinfo(L, "S", D);
      if (e->bpHandler(e->dbgRequest, D->currentline - 1, D->source + 1, e->bpOpaque))
        e->dbgBreak(L);
    }
    else if (e->bpHandler(DBG_BREAK, D->currentline - 1, NULL, e->bpOpaque))
    {
      lua_getinfo(L, "S", D);
      if (e->bpHandler(DBG_BREAK, D->currentline - 1, D->source + 1, e->bpOpaque))
        e->dbgBreak(L);
    }
  }
  if (D->event == LUA_HOOKCALL && e->dbgDepth >= 0)
    e->dbgDepth++;
  if (D->event == LUA_HOOKRET && e->dbgDepth >= 0)
    e->dbgDepth--;
}
void Engine::dbgBreak(lua_State* L, int mode)
{
  dbgRequest = mode;
  bpRun.reset();
  int prevTop = lua_gettop(L);
  luaLock.release();
  bpRun.wait();
  luaLock.acquire();
  if (handler)
    PostMessage(handler, WM_DBGCONTINUE, 0, 0);
  int newTop = lua_gettop(L);
  if (newTop != prevTop)
  {
    logMessage(String::format("Debugger modified stack size from %d to %d!", prevTop, newTop), LOG_ERROR);
    lua_settop(L, prevTop);
  }
  sethook(L);
}

void Engine::enqueue(Thread* t)
{
  dequeue(t);
  if (last_thread)
    last_thread->next = t;
  else
    first_thread = t;
  t->prev = last_thread;
  last_thread = t;
  nonEmptyQueue.set();
}
void Engine::dequeue(Thread* t)
{
  if (t->prev)
    t->prev->next = t->next;
  else if (t == first_thread)
    first_thread = t->next;
  else if (t == sleep_first)
    sleep_first = t->next;
  else if (t == suspend_first)
    suspend_first = t->next;
  if (t->next)
    t->next->prev = t->prev;
  else if (t == last_thread)
    last_thread = t->prev;
  else if (t == sleep_last)
    sleep_last = t->prev;
  else if (t == suspend_last)
    suspend_last = t->prev;
  t->next = NULL;
  t->prev = NULL;
}
void Engine::terminate(Thread* t)
{
  dequeue(t);
  if (t->tcounter)
  {
    add_counter(t->tcounter, t->epoch, 1);
    t->tcounter = 0;
  }
  t->release();
}
void Engine::sleep(Thread* t, uint32 time)
{
  dequeue(t);
  t->waketime = GetTickCount() + time;
  Thread* pos = sleep_first;
  while (pos && pos->waketime < t->waketime)
    pos = pos->next;
  t->next = pos;
  if (pos)
  {
    t->prev = pos->prev;
    pos->prev = t;
  }
  else
  {
    t->prev = sleep_last;
    sleep_last = t;
  }
  if (t->prev)
    t->prev->next = t;
  else
    sleep_first = t;
  nonEmptyQueue.set();
}
void Engine::suspend(Thread* t)
{
  dequeue(t);
  t->next = suspend_first;
  if (suspend_first)
    suspend_first->prev = t;
  else
    suspend_last = t;
  suspend_first = t;
  t->prev = NULL;
}
bool Engine::suspended(Thread* t)
{
  for (Thread* s = suspend_first; s; s = s->next)
    if (s == t)
      return true;
  return false;
}

bool Engine::load_module(char const* name)
{
  lock();

  if (modules.has(name))
  {
    unlock();
    return true;
  }
  WideString path(name);
  path.setExtension(L".dll");
  HMODULE module = LoadLibrary(path);
  if (module)
  {
    modules.set(name, module);
  
    typedef void (*Binder)(lua_State*);
    Binder bf = (Binder) GetProcAddress(module, "StartModule");
    if (bf)
      bf(cur_state());
  }

  unlock();

  return module != NULL;
}
void Engine::sethook(lua_State* L)
{
  int mask = LUA_MASKCOUNT;
  if (bpHandler)
  {
    if (hasBreakpoints || dbgRequest == DBG_STEP || dbgRequest == DBG_STEPIN || dbgRequest == DBG_STEPOUT)
      mask |= LUA_MASKLINE;
    if (dbgRequest == DBG_STEP)
      mask |= LUA_MASKCALL;
    if (dbgRequest == DBG_STEP || dbgRequest == DBG_STEPOUT)
      mask |= LUA_MASKRET;
  }
  lua_sethook(L, hook_func, mask, 20);
}
void Engine::setBreakpointHandler(BreakpointHandler handler, void* opaque)
{
  bpHandler = handler;
  bpOpaque = opaque;
}
void Engine::setBreakpoints(bool bp)
{
  if (L)
  {
    lock();
    hasBreakpoints = bp;
    sethook(cur_state());
    unlock();
  }
  else
    hasBreakpoints = bp;
}
void Engine::debugContinue(int reason)
{
  if (reason == DBG_HALT)
    halt();
  else if (reason != DBG_BREAK)
  {
    dbgThread = current_thread();
    dbgDepth = 0;
    dbgRequest = reason;
    bpRun.set();
  }
  else
  {
    lock();
    dbgThread = NULL;
    dbgDepth = 0;
    dbgRequest = DBG_STEPIN;
    sethook(cur_state());
    bpRun.set();
    unlock();
  }
}
void Engine::logMessage(char const* text, int type)
{
  if (handler)
  {
    size_t len = strlen(text);
    LogMessage* msg = (LogMessage*) malloc(sizeof(LogMessage) + len);
    msg->timestamp = sysTime();
    memcpy(msg->text, text, len + 1);
    PostMessage(handler, WM_ADDMESSAGE, (WPARAM) msg, type);
  }
}
Thread* Engine::getNextThread(Thread* thread, int& state)
{
  if (thread == NULL)
  {
    thread = first_thread;
    state = THREAD_YIELD;
    if (thread == NULL)
    {
      thread = sleep_first;
      state = THREAD_SLEEP;
    }
    if (thread == NULL)
    {
      thread = suspend_first;
      state = THREAD_SUSPEND;
    }
  }
  else if (thread->next)
  {
    if (state == THREAD_RUNNING)
    {
      if (thread == sleep_first)
        state = THREAD_SLEEP;
      else
        state = THREAD_YIELD;
    }
    thread = thread->next;
  }
  else
  {
    if (thread == last_thread)
    {
      thread = sleep_first;
      state = THREAD_SLEEP;
      if (thread == NULL)
      {
        thread = suspend_first;
        state = THREAD_SUSPEND;
      }
    }
    else if (thread == sleep_last)
    {
      thread = suspend_first;
      state = THREAD_SUSPEND;
    }
    else
      thread = NULL;
  }
  if (thread == cur_thread)
    state = THREAD_RUNNING;
  return thread;
}

}
