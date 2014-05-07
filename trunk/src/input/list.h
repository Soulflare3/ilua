#ifndef __INPUT_LIST__
#define __INPUT_LIST__

#include <windows.h>
#include "base/types.h"
#include "base/array.h"
#include "base/dictionary.h"
#include <lua/lua.hpp>

class InputList
{
  enum {keyMove = 0x00000010,
        mxRel   = 0x00000100,
        myRel   = 0x00000200
  };
  struct Key
  {
    uint32 time;
    uint32 flags;
    union
    {
      struct {float x; float y;};
      uint32 key;
    };
    Key()
      : time(0)
      , flags(0)
      , x(0)
      , y(0)
    {}
    Key(uint32 k)
      : time(0)
      , flags(0)
      , key(k)
      , y(0)
    {}
    Key(float mx, float my)
      : time(0)
      , flags(keyMove)
      , x(mx)
      , y(my)
    {}
  };
  int recordFrom;
  uint32 recording;
  uint32 lastTime;
  uint32 time;
  Array<Key> keys;
  static uint32 getmod();
  static void addlist(Array<INPUT>& inp, uint32 key, bool up);
  static void changemod(Array<INPUT>& inp, uint32 from, uint32 to);
  static Key parsekey(char const* key, Dictionary<uint32> const& keymap, int& pos);
public:
  InputList()
    : time(0)
    , recording(0)
  {}
  void addtext(char const* text, int length = -1);
  void addseq(char const* seq, Dictionary<uint32> const& keymap);
  void addkey(uint32 key)
  {
    keys.push(Key(key));
  }
  void addpos(float x, float y)
  {
    keys.push(Key(x, y));
  }
  void addposrel(float x, float y)
  {
    Key k(x, y);
    k.flags |= (mxRel | myRel);
    keys.push(k);
  }
  void delay(uint32 t)
  {
    if (keys.length())
    {
      time += t - keys.top().time;
      keys.top().time = t;
    }
  }

  uint32 getrecording() const
  {
    return recording;
  }
  void setrecording(uint32 id);
  void recordtime();

  int length() const
  {
    return keys.length();
  }
  uint32 duration() const
  {
    return time;
  }
  int match(int i, int key);

  void randomize(float f, bool rel);

  int itemtime(int i)
  {
    return keys[i].time;
  }
  void send(int from = 0, int to = -1);
  int sendex(int from);

  void dump(lua_State* L);
  void undump(size_t size, char const* ptr);

  static uint32 parsekey(char const* key, Dictionary<uint32> const& keymap);
  static uint32 curmod();
  static void unalt();
};

#endif // __INPUT_LIST__
