#ifndef __API_ENGINE__
#define __API_ENGINE__

#include "base/file.h"
#include "base/thread.h"
#include "ilua/ilua.h"
#include "base/dictionary.h"
#include "base/types.h"
#include "base/array.h"
#include "base/wstring.h"
#include <lua/lua.hpp>
#include <windows.h>

#define WM_OPENFILE       (WM_USER+110)
#define WM_ENGINESTART    (WM_USER+197)
#define WM_ENGINEHALT     (WM_USER+198)
#define WM_ADDMESSAGE     (WM_USER+199)
#define WM_DBGCONTINUE    (WM_USER+196)
#define DBG_LOADERROR     -2
#define DBG_HALT          -1
#define DBG_RUN           0
#define DBG_ERROR         1
#define DBG_BREAK         2
#define DBG_STEP          3
#define DBG_STEPIN        4
#define DBG_STEPOUT       5

#define THREAD_RUNNING    0
#define THREAD_YIELD      1
#define THREAD_SLEEP      2
#define THREAD_SUSPEND    3

#define LOG_OUTPUT        0
#define LOG_SYSTEM        1
#define LOG_ERROR         4
struct LogMessage
{
  uint64 timestamp;
  char text[1];
};
struct LoadedInfo
{
  WideString path;
  Array<int> validLines;
};

#define CO_MAGIC          0x1C2C3C4C

namespace api
{

typedef bool (*BreakpointHandler)(int, int, char const*, void*);

class Engine;

class Thread : public ilua::Thread
{
  lua_State* L;
  lua_State* origL;
  int first;
  int paused;
  int tid;
  void* dataptr;
public:
  Thread(int id, int narg);
  ~Thread()
  {
    if (dataptr) free(dataptr);
  }
  void suspend();
  int yield(lua_CFunction cont = NULL, int ctx = 0);
  int sleep(int time, lua_CFunction cont = NULL, int ctx = 0);
  void resume(Object* rc = NULL);
  void terminate();

  lua_State* state()
  {
    return L;
  }
  void setco(lua_State* S)
  {
    L = S;
  }
  int run();

  void lock()
  {
    locked++;
  }
  void unlock()
  {
    locked--;
  }

  int id() const
  {
    return tid;
  }
  void* newdata(int size)
  {
    if (dataptr) free(dataptr);
    dataptr = malloc(size);
    return dataptr;
  }
  void* data()
  {
    return dataptr;
  }

  Thread* next;
  Thread* prev;
  int locked;
  uint32 waketime;
  String name;
  int tcounter;
  int epoch;
  int tstatus;
  int status() const
  {
    return tstatus;
  }
};

class Engine : public ilua::Engine
{
private:
  lua_State* L;

  Thread* first_thread;
  Thread* last_thread;

  Thread* sleep_first;
  Thread* sleep_last;

  Thread* suspend_first;
  Thread* suspend_last;

  Thread* cur_thread;
  int cur_counter;
  uint32 thread_count;
  lua_State* cur_state()
  {
    return cur_thread ? cur_thread->state() : L;
  }

  HANDLE hThread;
  thread::Lock luaLock;
  thread::Counter lockRequest;
  thread::Event nonEmptyQueue;
  thread::Event bpRun;

  Dictionary<HMODULE> modules;
  HWND handler;

  BreakpointHandler bpHandler;
  void* bpOpaque;
  int dbgRequest;
  int dbgDepth;
  Thread* dbgThread;
  bool hasBreakpoints;
  bool hasKeepalive;
  void dbgBreak(lua_State* L, int mode = DBG_BREAK);

  bool shutdown;
  int exitCode;
  void dequeue(Thread* t);

  void sethook(lua_State* L);

  void resetvar();
  void start();
  void finish();
  void halt();

  static void bind(lua_State* L);

  int core_thread();
  static void hook_func(lua_State* L, lua_Debug* D);
  int add_counter(int id, int epoch, int value);
public:
  Engine();
  ~Engine();

  bool load_function(lua_State* cL, char const* file);
  ilua::Thread* load(char const* file);
  bool load_module(char const* name, char const* entry = NULL);

  lua_State* lock();
  void unlock();

  int get_counter(int id);
  void set_counter(int id, int value);
  Thread* create_thread(int narg, int counter = 0);
  Thread* current_thread()
  {
    return cur_thread;
  }

  void setHandler(HWND hWnd)
  {
    handler = hWnd;
  }
  void logMessage(char const* msg, int type);

  void enqueue(Thread* t);
  void suspend(Thread* t);
  void sleep(Thread* t, uint32 time);
  void terminate(Thread* t);
  bool suspended(Thread* t);
  void keepalive()
  {
    hasKeepalive = true;
  }

  void setBreakpointHandler(BreakpointHandler handler, void* opaque);
  void setBreakpoints(bool bp);

  void debugContinue(int reason);
  int debugState() const
  {
    if (dbgRequest == DBG_BREAK || dbgRequest == DBG_ERROR)
      return dbgRequest;
    return 0;
  }
  bool running() const
  {
    return L != NULL;
  }
  void exit(int code)
  {
    exitCode = code;
    shutdown = true;
  }

  Thread* getNextThread(Thread* thread, int& state);

  void resume()
  {
    bpRun.set();
  }
};

inline Engine* engine(lua_State* L)
{
  return (Engine*) ilua::engine(L);
}

}

#endif // __API_ENGINE__
