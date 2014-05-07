#include "slowop.h"

namespace ilua
{

static int slowop_gc(lua_State* L)
{
  SlowOperation** op = (SlowOperation**) lua_touserdata(L, 1);
  if (op) (*op)->~SlowOperation();
  return 0;
}

DWORD WINAPI SlowOperation::thread_proc(LPVOID arg)
{
  SlowOperation* op = (SlowOperation*) arg;
  op->run();
  if (!op->resumed)
  {
    op->e->lock();
    if (op->t)
    {
      op->t->resume(0);
      op->t->release();
    }
    op->t = NULL;
    op->e->unlock();
  }
  return 0;
}
lua_State* SlowOperation::output_begin()
{
  e->lock();
  return t->state();
}
void SlowOperation::output_end(int args)
{
  resumed = true;
  if (t)
  {
    t->resume(args);
    t->release();
  }
  t = NULL;
  e->unlock();
}
SlowOperation::~SlowOperation()
{
  if (hThread)
  {
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
  }
  if (t)
    t->release();
}
void SlowOperation::start(lua_State* L)
{
  *(SlowOperation**) lua_newuserdata(L, sizeof(SlowOperation*)) = this;
  lua_newtable(L);
  lua_pushcfunction(L, slowop_gc);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);
  e = engine(L);
  t = e->current_thread();
  if (t)
  {
    t->addref();
    t->suspend();
  }
  resumed = false;
  hThread = CreateThread(NULL, 0, thread_proc, this, 0, NULL);
}

}
