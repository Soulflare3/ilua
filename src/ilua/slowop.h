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
  void output_end(int args);

  virtual void run() {}
public:
  SlowOperation()
    : e(NULL), t(NULL), hThread(NULL), resumed(false)
  {}
  virtual ~SlowOperation();
  void start(lua_State* L);
};

}

#endif // __ILUA_SLOWOP__
