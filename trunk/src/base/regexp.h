#ifndef __BASE_REGEXP_H__
#define __BASE_REGEXP_H__

#include "base/types.h"
#include "base/string.h"
#include "base/array.h"

namespace re
{

typedef bool (*CharTraitFunc)(uint32);
class CharacterClass
{
  bool invert;
  Array<CharTraitFunc> funcs;
  struct Range
  {
    uint32 begin;
    uint32 end;
  };
  static int rangecomp(Range const& a, Range const& b);
  Array<Range> data;
  void addRange(uint32 a, uint32 b)
  {
    Range& r = data.push();
    r.begin = a;
    r.end = b;
  }
  void addClass(CharacterClass const& cls);
  void sort();
public:
  CharacterClass()
    : invert(false)
  {}
  CharacterClass(CharTraitFunc func, bool inv = false)
    : invert(inv)
  {
    funcs.push(func);
  }
  CharacterClass(char const* src, int flags = 0)
  {
    init(src, flags);
  }

  uint8_const_ptr init(char const* src, int flags = 0);

  bool match(uint32 c) const;

  static CharacterClass* getDefault(char ch, int flags = 0);
};

struct Match
{
  char const* start[32];
  char const* end[32];
};
struct Thread;
struct State;

class Prog
{
  uint32 flags;
  State* start;
  State* states;
  int numStates;
  Array<CharacterClass*> masks;
  int numCaptures;

  int maxThreads;
  Thread* threads;
  int cur;
  int numThreads[2];
  char const* matchText;
  void addthread(State* state, Match const& match);
  void advance(State* state, Match const& match, uint32 cp, char const* ref);
public:
  Prog(char const* expr, int length = -1, uint32 flags = 0);
  ~Prog();

  enum {CaseInsensitive = 0x01,
        DotAll          = 0x02,
        MultiLine       = 0x04,
        Unicode         = 0x08,
  };

  bool match(char const* text, Array<String>* sub = NULL);
  int find(char const* text, int start = 0, Array<String>* sub = NULL);
  String replace(char const* text, char const* with);

  int captures() const
  {
    return numCaptures;
  }
  int run(char const* text, int length, bool exact, bool (*callback) (Match const& match, void* arg), void* arg);
};

}

#endif // __BASE_REGEXP_H__
