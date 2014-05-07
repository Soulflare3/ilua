#include "list.h"
#include "keylist.h"
#include "base/utils.h"
#include "base/checksum.h"

enum {lmAlt     = 0x03,
      lmLAlt    = 0x01,
      lmRAlt    = 0x02,
      lmShift   = 0x0C,
      lmLShift  = 0x04,
      lmRShift  = 0x08,
      lmCtrl    = 0x30,
      lmLCtrl   = 0x10,
      lmRCtrl   = 0x20,
      lmWin     = 0xC0,
      lmLWin    = 0x40,
      lmRWin    = 0x80
};

InputList::Key InputList::parsekey(char const* seq, Dictionary<uint32> const& keymap, int& pos)
{
  if (!seq[pos]) return Key(0);

  Key key;
  key.time = 0;
  key.flags = 0;

  while (seq[pos] && s_isspace(seq[pos]))
    pos++;
  if (seq[pos] == '{')
  {
    key.time = int(atof(seq + pos + 1) * 1000);
    while (seq[pos] && seq[pos] != '}')
      pos++;
    if (seq[pos])
      pos++;
  }
  while (seq[pos] && s_isspace(seq[pos]))
    pos++;
  if (seq[pos] == '<')
  {
    pos++;
    key.flags |= keyMove;
    if (seq[pos] == '+' || seq[pos] == '-')
      key.flags |= mxRel;
    key.x = atof(seq + pos);
    while (seq[pos] && seq[pos] != ',')
      pos++;
    if (seq[pos])
      pos++;
    if (seq[pos] == '+' || seq[pos] == '-')
      key.flags |= myRel;
    key.y = atof(seq + pos);
    while (seq[pos] && seq[pos] != '>')
      pos++;
    if (seq[pos])
      pos++;
  }
  else
  {
    uint32 mod = 0;
    if (seq[pos] == '(' && (seq[pos + 1] == '-' || seq[pos + 1] == '+') && seq[pos + 2] == ')')
    {
      if (seq[pos + 1] == '+')
        mod = mDOWN;
      else if (seq[pos + 1] == '-')
        mod = mUP;
      pos += 3;
    }
    while (seq[pos] && s_isspace(seq[pos]))
      pos++;
    while (seq[pos] == '!' || seq[pos] == '^' || seq[pos] == '+' || seq[pos] == '#')
    {
      switch (seq[pos++])
      {
      case '!': mod |= mALT; break;
      case '^': mod |= mCTRL; break;
      case '+': mod |= mSHIFT; break;
      case '#': mod |= mWIN; break;
      }
    }
    while (seq[pos] && s_isspace(seq[pos]))
      pos++;
    int end = pos;
    while (seq[end] && !s_isspace(seq[end]))
      end++;
    String str(seq + pos, end - pos);
    while (str.length() > 0)
    {
      if (keymap.has(str))
        break;
      str.resize(str.length() - 1);
      end--;
    }
    pos = end;
    if (str.length() > 0)
      key.key = keymap.get(str) | mod;
  }
  return key;
}
uint32 InputList::parsekey(char const* key, Dictionary<uint32> const& keymap)
{
  int pos = 0;
  Key k = parsekey(key, keymap, pos);
  return (k.flags == 0 ? k.key : 0);
}

void InputList::addseq(char const* seq, Dictionary<uint32> const& keymap)
{
  int pos = 0;
  while (seq[pos])
  {
    Key key = parsekey(seq, keymap, pos);
    if (key.flags || key.key)
    {
      time += key.time;
      keys.push(key);
    }
  }
}
void InputList::addtext(char const* text, int length)
{
  if (length < 0) length = strlen(text);
  for (int i = 0; i < length; i++)
  {
    uint32 key = chartokey[(uint32) text[i]];
    if (key)
      keys.push(Key(key));
  }
}
static int key2mod(int key)
{
  switch (key)
  {
  case VK_MENU:       return lmAlt;
  case VK_LMENU:      return lmLAlt;
  case VK_RMENU:      return lmRAlt;
  case VK_SHIFT:      return lmShift;
  case VK_LSHIFT:     return lmLShift;
  case VK_RSHIFT:     return lmRShift;
  case VK_CONTROL:    return lmCtrl;
  case VK_LCONTROL:   return lmLCtrl;
  case VK_RCONTROL:   return lmRCtrl;
  case VK_LWIN:       return lmLWin;
  case VK_RWIN:       return lmRWin;
  default:            return 0;
  }
}
void InputList::send(int from, int to)
{
  if (to < 0) to = keys.length();
  Array<INPUT> inp;
  uint32 mod = getmod();
  uint32 omod = mod;
  uint32 fmod = 0;
  for (int i = 0; i < from; i++)
  {
    if (keys[i].flags == 0)
    {
      if (keys[i].key & mUP)
        fmod &= ~key2mod(keys[i].key & 0xFF);
      else if (keys[i].key & mDOWN)
      {
        uint32 key = (keys[i].key & 0xFF);
        if (key == VK_MENU)
          key = ((mod & lmAlt) == lmRAlt ? VK_RMENU : VK_LMENU);
        else if (key == VK_SHIFT)
          key = ((mod & lmShift) == lmRShift ? VK_RSHIFT : VK_LSHIFT);
        else if (key == VK_CONTROL)
          key = ((mod & lmCtrl) == lmRCtrl ? VK_RCONTROL : VK_LCONTROL);
        fmod |= key2mod(key);
      }
    }
  }
  for (int i = from; i < to; i++)
  {
    if (keys[i].flags & keyMove)
    {
      if (inp.length() > 0)
        SendInput(inp.length(), &inp[0], sizeof(INPUT));
      inp.clear();

      POINT pt;
      GetCursorPos(&pt);
      int dx, dy;
      if (keys[i].x > -1 && keys[i].x < 1)
        dx = int(GetSystemMetrics(SM_CXSCREEN) * keys[i].x);
      else
        dx = int(keys[i].x);
      if (keys[i].y > -1 && keys[i].y < 1)
        dy = int(GetSystemMetrics(SM_CYSCREEN) * keys[i].y);
      else
        dy = int(keys[i].y);
      if (keys[i].flags & mxRel)
        dx += pt.x;
      if (keys[i].flags & myRel)
        dy += pt.y;
      SetCursorPos(dx, dy);
    }
    else
    {
      if (keys[i].key & mUP)
      {
        uint32 cmod = key2mod(keys[i].key & 0xFF);
        omod &= ~cmod;
        fmod &= ~cmod;
        mod &= ~cmod;
        addlist(inp, keys[i].key & 0xFF, true);
      }
      else
      {
        uint32 cmod = 0;
        if (keys[i].key & mALT)
          cmod |= ((mod & lmAlt) ? (mod & lmAlt) : lmLAlt);
        if (keys[i].key & mSHIFT)
          cmod |= ((mod & lmShift) ? (mod & lmShift) : lmLShift);
        if (keys[i].key & mCTRL)
          cmod |= ((mod & lmCtrl) ? (mod & lmCtrl) : lmLCtrl);
        if (keys[i].key & mWIN)
          cmod |= ((mod & lmWin) ? (mod & lmWin) : lmLWin);
        changemod(inp, mod, fmod | cmod);
        mod = fmod | cmod;
        uint32 key = (keys[i].key & 0xFF);
        if (key == VK_MENU)
          key = ((mod & lmAlt) == lmRAlt ? VK_RMENU : VK_LMENU);
        else if (key == VK_SHIFT)
          key = ((mod & lmShift) == lmRShift ? VK_RSHIFT : VK_LSHIFT);
        else if (key == VK_CONTROL)
          key = ((mod & lmCtrl) == lmRCtrl ? VK_RCONTROL : VK_LCONTROL);
        addlist(inp, key, false);
        if ((keys[i].key & mDOWN) == 0)
          addlist(inp, key, true);
        cmod = key2mod(key);
        if (keys[i].key & mDOWN)
        {
          fmod |= cmod;
          omod |= cmod;
          mod |= cmod;
        }
        else
          mod &= ~cmod;
      }
    }
  }
  changemod(inp, mod, omod);
  if (inp.length() > 0)
    SendInput(inp.length(), &inp[0], sizeof(INPUT));
}
int InputList::sendex(int from)
{
  int to = from + 1;
  while (to < keys.length() && keys[to].time == 0)
    to++;
  send(from, to);
  return to;
}
int InputList::match(int pos, int key)
{
  if (pos >= keys.length())
    return 0;
  if (keys[pos].flags)
    return 0;
  if (key >= 0 && (keys[pos].key == key || keys[pos].key == (key | mDOWN)))
    return pos + 1;
  if (key < 0 && keys[pos].key == ((-key) | mUP))
    return pos + 1;
  if (key >= 0)
    return 0;
  for (int i = pos - 1; i >= 0; i--)
    if ((keys[i].key & 0xF0FF) == (((-key) & 0xFF) | mDOWN))
      return 0;
  return pos;
}

void InputList::addlist(Array<INPUT>& inp, uint32 key, bool up)
{
  INPUT& i = inp.push();
  if (mousekeys[key])
  {
    i.type = INPUT_MOUSE;
    i.mi.dx = 0;
    i.mi.dy = 0;
    i.mi.mouseData = 0;
    switch (key)
    {
    case VK_LBUTTON:
      i.mi.dwFlags = (up ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN);
      break;
    case VK_RBUTTON:
      i.mi.dwFlags = (up ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN);
      break;
    case VK_MBUTTON:
      i.mi.dwFlags = (up ? MOUSEEVENTF_MIDDLEUP : MOUSEEVENTF_MIDDLEDOWN);
      break;
    case VK_XBUTTON1:
      i.mi.dwFlags = (up ? MOUSEEVENTF_XUP : MOUSEEVENTF_XDOWN);
      i.mi.mouseData = XBUTTON1;
      break;
    case VK_XBUTTON2:
      i.mi.dwFlags = (up ? MOUSEEVENTF_XUP : MOUSEEVENTF_XDOWN);
      i.mi.mouseData = XBUTTON2;
      break;
    case VK_WHEELLEFT:
      i.mi.dwFlags = MOUSEEVENTF_HWHEEL;
      i.mi.mouseData = -WHEEL_DELTA;
      break;
    case VK_WHEELRIGHT:
      i.mi.dwFlags = MOUSEEVENTF_HWHEEL;
      i.mi.mouseData = WHEEL_DELTA;
      break;
    case VK_WHEELUP:
      i.mi.dwFlags = MOUSEEVENTF_WHEEL;
      i.mi.mouseData = WHEEL_DELTA;
      break;
    case VK_WHEELDOWN:
      i.mi.dwFlags = MOUSEEVENTF_WHEEL;
      i.mi.mouseData = -WHEEL_DELTA;
      break;
    }
    i.mi.time = 0;
    i.mi.dwExtraInfo = 0;
  }
  else
  {
    i.type = INPUT_KEYBOARD;
    i.ki.wVk = key;
    i.ki.wScan = 0;
    i.ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0);
    i.ki.time = 0;
    i.ki.dwExtraInfo = 0;
  }
}
void InputList::changemod(Array<INPUT>& inp, uint32 from, uint32 to)
{
  //bool fake_ctrl = ((!(from & lmAlt)) != (!(to & lmAlt)) && ((from | to) & lmCtrl) == 0);
  bool fake_ctrl = false;
    
  if (fake_ctrl || ((from & lmLCtrl) == 0 && (to & lmLCtrl) != 0))
    addlist(inp, VK_LCONTROL, false);
  if (((from & lmRCtrl) == 0 && (to & lmRCtrl) != 0))
    addlist(inp, VK_RCONTROL, false);

  if ((from ^ to) & lmLAlt)
    addlist(inp, VK_LMENU, (from & lmLAlt) != 0);
  if ((from ^ to) & lmRAlt)
    addlist(inp, VK_RMENU, (from & lmRAlt) != 0);
  if ((from ^ to) & lmLShift)
    addlist(inp, VK_LSHIFT, (from & lmLShift) != 0);
  if ((from ^ to) & lmRShift)
    addlist(inp, VK_RSHIFT, (from & lmRShift) != 0);
  if ((from ^ to) & lmLWin)
    addlist(inp, VK_LWIN, (from & lmLWin) != 0);
  if ((from ^ to) & lmRWin)
    addlist(inp, VK_RWIN, (from & lmRWin) != 0);

  if (fake_ctrl || ((from & lmLCtrl) != 0 && (to & lmLCtrl) == 0))
    addlist(inp, VK_LCONTROL, true);
  if (((from & lmRCtrl) != 0 && (to & lmRCtrl) == 0))
    addlist(inp, VK_RCONTROL, true);
}
void InputList::unalt()
{
  INPUT inp[3];
  memset(inp, 0, sizeof inp);
  inp[0].type = INPUT_KEYBOARD;
  inp[0].ki.wVk = VK_CONTROL;
  inp[1].type = INPUT_KEYBOARD;
  inp[1].ki.wVk = VK_MENU;
  inp[1].ki.dwFlags = KEYEVENTF_KEYUP;
  inp[2].type = INPUT_KEYBOARD;
  inp[2].ki.wVk = VK_CONTROL;
  inp[2].ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(3, inp, sizeof(INPUT));
}
uint32 InputList::getmod()
{
  uint32 mod = 0;
  if (GetAsyncKeyState(VK_LMENU) & 0x8000)
    mod |= lmLAlt;
  if (GetAsyncKeyState(VK_RMENU) & 0x8000)
    mod |= lmRAlt;
  if (GetAsyncKeyState(VK_LCONTROL) & 0x8000)
    mod |= lmLCtrl;
  if (GetAsyncKeyState(VK_RCONTROL) & 0x8000)
    mod |= lmRCtrl;
  if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
    mod |= lmLShift;
  if (GetAsyncKeyState(VK_RSHIFT) & 0x8000)
    mod |= lmRShift;
  if (GetAsyncKeyState(VK_LWIN) & 0x8000)
    mod |= lmLWin;
  if (GetAsyncKeyState(VK_RWIN) & 0x8000)
    mod |= lmRWin;
  return mod;
}
uint32 InputList::curmod()
{
  uint32 mod = 0;
  if (GetAsyncKeyState(VK_MENU) & 0x8000)
    mod |= mALT;
  if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
    mod |= mCTRL;
  if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
    mod |= mSHIFT;
  if ((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000))
    mod |= mWIN;
  return mod;
}

void InputList::randomize(float f, bool rel)
{
  time = 0;
  for (int i = 0; i < keys.length(); i++)
  {
    float r = 1 - f + 2 * f * frand();
    if (rel)
      keys[i].time = uint32(float(keys[i].time) * r);
    else
      keys[i].time = uint32(1000.0f * r);
    time += keys[i].time;
  }
}
void InputList::setrecording(uint32 id)
{
  if (id)
  {
    recording = id;
    lastTime = GetTickCount();
    recordFrom = keys.length();
  }
  else
  {
    recording = 0;
    uint8 mask[256];
    memset(mask, 0, sizeof mask);
    uint32 taccum = 0;
    for (int i = recordFrom; i < keys.length(); i++)
    {
      keys[i].time += taccum;
      taccum = 0;
      if (keys[i].flags == 0)
      {
        if (keys[i].key & mUP)
        {
          if (mask[keys[i].key & 0xFF])
            mask[keys[i].key & 0xFF] = 0;
          else
          {
            taccum += keys[i].time;
            keys.remove(i);
            i--;
          }
        }
        else if (keys[i].key & mDOWN)
          mask[keys[i].key & 0xFF] = 1;
      }
    }
    for (int i = keys.length() - 1; i >= recordFrom; i--)
    {
      if (keys[i].flags == 0)
      {
        if ((keys[i].key & mDOWN) && mask[keys[i].key & 0xFF])
        {
          mask[keys[i].key & 0xFF] = 0;
          if (i < keys.length() - 1)
            keys[i + 1].time += keys[i].time;
          keys.remove(i);
        }
      }
    }
  }
}
void InputList::recordtime()
{
  if (recording)
  {
    uint32 cur = GetTickCount();
    delay(cur - lastTime);
    lastTime = cur;
  }
}
void InputList::dump(lua_State* L)
{
  luaL_Buffer b;
  uint32 size = keys.length() * sizeof(Key);
  uint64 sum = jenkins(&keys[0], size);
  char* ptr = luaL_buffinitsize(L, &b, size + sizeof(sum));
  memcpy(ptr, &sum, sizeof(sum));
  memcpy(ptr + sizeof(sum), &keys[0], size);
  luaL_pushresultsize(&b, size + sizeof(sum));
}
void InputList::undump(size_t size, char const* ptr)
{
  if (ptr && size >= sizeof(uint64))
  {
    uint64 sum = *(uint64*) ptr;
    if (sum == jenkins(ptr + sizeof(sum), size - sizeof(sum)))
    {
      for (size_t p = sizeof(sum); p + sizeof(Key) <= size; p += sizeof(Key))
      {
        Key* k = (Key*) (ptr + p);
        keys.push(*k);
        time += k->time;
      }
    }
  }
}
