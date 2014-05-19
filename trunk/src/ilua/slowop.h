#ifndef __ILUA_SLOWOP__
#define __ILUA_SLOWOP__

#include <windows.h>
#include "ilua.h"

namespace ilua
{

class SlowOperation
{
  Engine* e;
  Thread* t;
  HANDLE hThread;
  bool resumed;
  static DWORD WINAPI thread_proc(LPVOID arg);
protected:
  lua_State* output_begin();
  void output_end();

  virtual void run() = 0;
public:
  SlowOperation()
    : e(NULL), t(NULL), hThread(NULL), resumed(false)
  {}
  virtual ~SlowOperation();
  int start(lua_State* L);

  void* operator new(size_t sz, lua_State* L);
  void operator delete(void* ptr, lua_State* L)
  {}
};

}

#endif // __ILUA_SLOWOP__
