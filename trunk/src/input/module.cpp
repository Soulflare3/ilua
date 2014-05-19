#include "module.h"
#include "keylist.h"
#include "list.h"

#define IM_ADDHOTKEY      (WM_USER+100)
#define IM_SETHOOK        (WM_USER+101)
#define IM_ONHOOK         (WM_USER+102)
#define IM_ONHOOK_HK      (WM_USER+103)
#define IM_ADDSEQ         (WM_USER+104)
#define IM_HOTKEY         (WM_USER+105)
#define IM_ADDRECORD      (WM_USER+106)
#define IM_REMRECORD      (WM_USER+107)

struct InputModule::Sequence
{
  InputList list;
  Array<bool> match;
  int func;
};

InputModule::InputModule()
  : hWorkThread(NULL)
  , hHookThread(NULL)
  , tid(0)
  , tHookId(0)
  , hookfunc(0)
  , threadReady(false)
  , keymap(DictionaryMap::asciiNoCase)
{
  GetKeyboardState(keyTable);
  memset(binds, 0, sizeof binds);
  for (uint32 i = 0; i < 256; i++)
    if (keynames[i])
      keymap.set(keynames[i], i);
}
InputModule::~InputModule()
{
  if (hWorkThread)
  {
    PostThreadMessage(tid, WM_QUIT, 0, 0);
    CloseHandle(hWorkThread);
  }
  if (hHookThread)
  {
    PostThreadMessage(tHookId, WM_QUIT, 0, 0);
    CloseHandle(hHookThread);
  }
  for (int i = 0; i < sequences.length(); i++)
    delete sequences[i];
}

InputModule* InputModule::get(lua_State* L)
{
  lua_getfield(L, LUA_REGISTRYINDEX, MODULENAME);
  InputModule* m = ilua::toobject<InputModule>(L, -1, MODULENAME);
  lua_pop(L, 1);
  return m;
}
uint32 InputModule::thread_id()
{
  if (hWorkThread == NULL)
  {
    hWorkThread = thread::create(this, &InputModule::thread_proc, &tid);
    hHookThread = thread::create(this, &InputModule::hook_proc, &tHookId);
    threadReady.wait();
  }
  return tid;
}

int InputModule::hook_proc()
{
  HMODULE thisModule = GetModuleHandle(L"input");
  SetMessageExtraInfo((LPARAM) this);
  SetWindowsHookEx(WH_KEYBOARD_LL, keybd_proc, thisModule, 0);
  SetWindowsHookEx(WH_MOUSE_LL, mouse_proc, thisModule, 0);
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0) != 0)
    ;
  return 0;
}
LRESULT CALLBACK InputModule::keybd_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
  InputModule* m = (InputModule*) GetMessageExtraInfo();
  if (nCode < 0 || m == NULL)
    CallNextHookEx(0, nCode, wParam, lParam);
  KBDLLHOOKSTRUCT* kbd = (KBDLLHOOKSTRUCT*) lParam;
  int code = 0;
  if ((kbd->flags & LLKHF_INJECTED) == 0)
  {
    if (wParam == WM_KEYUP)
      m->keyTable[kbd->vkCode] = 0x00;
    else
      m->keyTable[kbd->vkCode] = 0x80;
    if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
      code = (kbd->vkCode | InputList::curmod() | mUP);
    else if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
      code = (kbd->vkCode | InputList::curmod());
    uint32 msg = IM_ONHOOK;
    if (m->binds[code])
    {
      if ((code & mALT) && !(code & mCTRL))
        InputList::unalt();
      PostThreadMessage(m->tid, IM_HOTKEY, code, 0);
      msg = IM_ONHOOK_HK;
    }
    else
      code = 0;
    PostThreadMessage(m->tid, msg, wParam, kbd->vkCode);
  }
  if (code == 0)
  {
    kbd->flags &= ~LLKHF_INJECTED;
    return CallNextHookEx(0, nCode, wParam, lParam);
  }
  return TRUE;
}
LRESULT CALLBACK InputModule::mouse_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
  InputModule* m = (InputModule*) GetMessageExtraInfo();
  if (nCode < 0 || m == NULL)
    return CallNextHookEx(0, nCode, wParam, lParam);
  MSLLHOOKSTRUCT* ms = (MSLLHOOKSTRUCT*) lParam;
  int code = 0;
  if ((ms->flags & LLMHF_INJECTED) == 0)
  {
    switch (wParam)
    {
    case WM_LBUTTONDOWN:
      code = VK_LBUTTON | InputList::curmod();
      break;
    case WM_LBUTTONUP:
      code = VK_LBUTTON | InputList::curmod() | mUP;
      break;
    case WM_RBUTTONDOWN:
      code = VK_RBUTTON | InputList::curmod();
      break;
    case WM_RBUTTONUP:
      code = VK_RBUTTON | InputList::curmod() | mUP;
      break;
    case WM_MBUTTONDOWN:
      code = VK_MBUTTON | InputList::curmod();
      break;
    case WM_MBUTTONUP:
      code = VK_MBUTTON | InputList::curmod() | mUP;
      break;
    case WM_XBUTTONDOWN:
      code = (HIWORD(ms->mouseData) == XBUTTON2 ? VK_XBUTTON2 : VK_XBUTTON1) | InputList::curmod();
      break;
    case WM_XBUTTONUP:
      code = (HIWORD(ms->mouseData) == XBUTTON2 ? VK_XBUTTON2 : VK_XBUTTON1) | InputList::curmod() | mUP;
      break;
    case WM_MOUSEWHEEL:
      code = (GET_WHEEL_DELTA_WPARAM(ms->mouseData) > 0 ? VK_WHEELUP : VK_WHEELDOWN) | InputList::curmod();
      break;
    case WM_MOUSEHWHEEL:
      code = (GET_WHEEL_DELTA_WPARAM(ms->mouseData) > 0 ? VK_WHEELRIGHT : VK_WHEELLEFT) | InputList::curmod();
      break;
    }
    uint32 msg = IM_ONHOOK;
    if (m->binds[code])
    {
      if ((code & mALT) && !(code & mCTRL))
        InputList::unalt();
      PostThreadMessage(m->tid, IM_HOTKEY, code, 0);
      msg = IM_ONHOOK_HK;
    }
    else
      code = 0;
    PostThreadMessage(m->tid, msg, wParam, (wParam == WM_MOUSEMOVE ?
      MAKELONG(ms->pt.x, ms->pt.y) : ms->mouseData));
  }
  if (code == 0)
  {
    ms->flags &= ~LLMHF_INJECTED;
    return CallNextHookEx(0, nCode, wParam, lParam);
  }
  return TRUE;
}

void InputModule::addsequence(char const* seq, bool text, int func)
{
  Sequence* s = new Sequence;
  if (text)
    s->list.addtext(seq, -1);
  else
    s->list.addseq(seq, keymap);
  if (s->list.length() == 0)
  {
    delete s;
    return;
  }
  s->match.resize(s->list.length() + 1, false);
  s->match[0] = true;
  s->func = func;
  PostThreadMessage(thread_id(), IM_ADDSEQ, (WPARAM) s, 0);
}

int InputModule::thread_proc()
{
  MSG msg;

  PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);
  threadReady.set();

  while (GetMessage(&msg, NULL, 0, 0) != 0)
  {
    switch (msg.message)
    {
    case IM_ADDHOTKEY:
      {
        int id = (msg.lParam & 0x1FFF);
        int limit = ((msg.lParam >> 16) & 0xFFFF);
        if (limit == 0) limit = 0xFFFF;
        lua_State* L = e->lock();
        if (binds[id])
          luaL_unref(L, LUA_REGISTRYINDEX, binds[id]);
        binds[id] = msg.wParam;
        e->set_counter(binds[id], limit);
        e->unlock();
      }
      break;
    case IM_SETHOOK:
      if (hookfunc)
      {
        lua_State* L = e->lock();
        luaL_unref(L, LUA_REGISTRYINDEX, hookfunc);
        e->unlock();
      }
      hookfunc = msg.wParam;
      break;
    case IM_ONHOOK:
    case IM_ONHOOK_HK:
      {
        int code = 0;
        switch (msg.wParam)
        {
        case WM_KEYUP:
        case WM_SYSKEYUP:
          code = -msg.lParam;
          break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
          code = msg.lParam;
          break;
        case WM_LBUTTONDOWN:
          code = VK_LBUTTON;
          break;
        case WM_LBUTTONUP:
          code = -VK_LBUTTON;
          break;
        case WM_RBUTTONDOWN:
          code = VK_RBUTTON;
          break;
        case WM_RBUTTONUP:
          code = -VK_RBUTTON;
          break;
        case WM_MBUTTONDOWN:
          code = VK_MBUTTON;
          break;
        case WM_MBUTTONUP:
          code = -VK_MBUTTON;
          break;
        case WM_XBUTTONDOWN:
          code = (HIWORD(msg.lParam) == XBUTTON2 ? VK_XBUTTON2 : VK_XBUTTON1);
          break;
        case WM_XBUTTONUP:
          code = -(HIWORD(msg.lParam) == XBUTTON2 ? VK_XBUTTON2 : VK_XBUTTON1);
          break;
        case WM_MOUSEWHEEL:
          code = (GET_WHEEL_DELTA_WPARAM(msg.lParam) > 0 ? VK_WHEELUP : VK_WHEELDOWN);
          break;
        case WM_MOUSEHWHEEL:
          code = (GET_WHEEL_DELTA_WPARAM(msg.lParam) > 0 ? VK_WHEELRIGHT : VK_WHEELLEFT);
          break;
        }
        if (code && (hookfunc || sequences.length() || (msg.message != IM_ONHOOK_HK && recording.length())))
        {
          lua_State* L = e->lock();
          if (L)
          {
            for (int i = 0; i < sequences.length(); i++)
            {
              Sequence& seq = *sequences[i];
              Array<bool> tmp;
              tmp.resize(seq.list.length() + 1, false);
              tmp[0] = true;
              for (int j = 0; j <= seq.list.length(); j++)
                if (seq.match[j])
                  tmp[seq.list.match(j, code)] = true;
              seq.match = tmp;
              if (seq.match[seq.list.length()])
              {
                lua_rawgeti(L, LUA_REGISTRYINDEX, seq.func);
                e->create_thread(0);
              }
            }
            if (msg.message != IM_ONHOOK_HK)
            {
              for (int i = 0; i < recording.length(); i++)
              {
                lua_rawgeti(L, LUA_REGISTRYINDEX, recording[i]);
                InputList* list = (InputList*) lua_touserdata(L, -1);
                if (list)
                {
                  list->addkey(code > 0 ? (code | mDOWN) : ((-code) | mUP));
                  list->recordtime();
                }
                lua_pop(L, 1);
              }
            }
            if (hookfunc)
            {
              lua_rawgeti(L, LUA_REGISTRYINDEX, hookfunc);
              lua_pushinteger(L, code > 0 ? code : -code);
              lua_pushboolean(L, code > 0);
              e->create_thread(2);
            }
          }
          e->unlock();
        }
        else if (msg.wParam == WM_MOUSEMOVE && recording.length())
        {
          lua_State* L = e->lock();
          if (L)
          {
            for (int i = 0; i < recording.length(); i++)
            {
              lua_rawgeti(L, LUA_REGISTRYINDEX, recording[i]);
              InputList* list = (InputList*) lua_touserdata(L, -1);
              if (list)
              {
                list->addpos((float) LOWORD(msg.lParam), (float) HIWORD(msg.lParam));
                list->recordtime();
              }
              lua_pop(L, 1);
            }
          }
          e->unlock();
        }
      }
      break;
    case IM_HOTKEY:
      if (binds[msg.wParam])
      {
        lua_State* L = e->lock();
        int key = (msg.wParam & 0x1FFF);
        if (L)
        {
          int counter = e->get_counter(binds[key]);
          if (counter)
          {
            lua_rawgeti(L, LUA_REGISTRYINDEX, binds[key]);
            lua_pushinteger(L, msg.wParam);
            e->create_thread(1, binds[key]);
          }
        }
        e->unlock();
      }
      break;
    case IM_ADDSEQ:
      sequences.push((Sequence*) msg.wParam);
      break;
    case IM_ADDRECORD:
      recording.push(msg.wParam);
      break;
    case IM_REMRECORD:
      {
        uint32 pos;
        for (pos = 0; pos < recording.length() && recording[pos] != msg.wParam; pos++)
          ;
        if (pos < recording.length())
        {
          recording.remove(pos);
          lua_State* L = e->lock();
          lua_rawgeti(L, LUA_REGISTRYINDEX, msg.wParam);
          InputList* list = (InputList*) lua_touserdata(L, -1);
          if (list)
            list->setrecording(0);
          lua_pop(L, 1);
          luaL_unref(L, LUA_REGISTRYINDEX, msg.wParam);
          e->unlock();
        }
        if (msg.lParam)
          ((ilua::Thread*) msg.lParam)->resume();
      }
      break;
    }
  }

  return 0;
}

void InputModule::bind(uint32 key, int handler, int limit)
{
  PostThreadMessage(thread_id(), IM_ADDHOTKEY, handler, (key & 0x1FFF) | (limit << 16));
}
void InputModule::sethook(int handler)
{
  PostThreadMessage(thread_id(), IM_SETHOOK, handler, 0);
}

int InputModule::recordstart(lua_State* L, int idx)
{
  InputList* list = (InputList*) lua_touserdata(L, idx);
  if (list->getrecording() == 0)
  {
    lua_pushvalue(L, idx);
    uint32 id = luaL_ref(L, LUA_REGISTRYINDEX);
    list->setrecording(id);
    PostThreadMessage(thread_id(), IM_ADDRECORD, id, 0);
  }
  return 0;
}
int InputModule::recordstop(lua_State* L, int idx)
{
  InputList* list = (InputList*) lua_touserdata(L, idx);
  uint32 id = list->getrecording();
  if (id)
  {
    e->current_thread()->suspend();
    PostThreadMessage(thread_id(), IM_REMRECORD, id, (LPARAM) e->current_thread());
    return lua_yield(L, 0);
  }
  return 0;
}
