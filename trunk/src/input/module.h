#ifndef __INPUT_MODULE__
#define __INPUT_MODULE__

#include <windows.h>
#include "ilua/ilua.h"
#include "base/thread.h"
#include "base/types.h"
#include "base/dictionary.h"

#define MODULENAME    "ilua_input"

class InputModule : public ilua::Object
{
  thread::Event threadReady;
  Dictionary<uint32> keymap;
  HANDLE hWorkThread;
  HANDLE hHookThread;
  uint32 tid;
  uint32 tHookId;

  int binds[0x2000];
  int hookfunc;
  struct Sequence;
  Array<Sequence*> sequences;
  Array<uint32> recording;

  int thread_proc();
  int hook_proc();
  static LRESULT CALLBACK keybd_proc(int nCode, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK mouse_proc(int nCode, WPARAM wParam, LPARAM lParam);
public:
  InputModule();
  ~InputModule();

  uint32 thread_id();
  static InputModule* get(lua_State* L);

  void bind(uint32 key, int handler, int limit = 0);
  void sethook(int handler);

  int recordstart(lua_State* L, int idx);
  int recordstop(lua_State* L, int idx);

  Dictionary<uint32> const& getkeymap() const
  {
    return keymap;
  }

  void addsequence(char const* seq, bool text, int func);

  uint32 nametokey(char const* key)
  {
    return keymap.get(key);
  }
};

void bind_mouse(lua_State* L);

#endif // __INPUT_MODULE__
