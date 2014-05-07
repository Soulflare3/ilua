#include <string.h>

#include "regexp.h"
#include "utf8.h"

namespace re
{

static uint32 unescape(uint8_const_ptr& chr)
{
  uint32 cp = 0;
  switch (*chr)
  {
  case 'a': chr++; return '\a';
  case 'b': chr++; return '\b';
  case 'f': chr++; return '\f';
  case 'n': chr++; return '\n';
  case 'r': chr++; return '\r';
  case 't': chr++; return '\t';
  case 'v': chr++; return '\v';
  case 'u':
    chr++;
    for (int z = 0; z < 4 && s_isxdigit(*chr); z++)
    {
      cp *= 16;
      if (*chr >= '0' && *chr <= '9') cp += int(*chr - '0');
      else if (*chr >= 'a' && *chr <= 'f') cp += 10 + int(*chr - 'a');
      else if (*chr >= 'A' && *chr <= 'F') cp += 10 + int(*chr - 'A');
      chr++;
    }
    return cp;
  default:
    if (s_isdigit(*chr))
    {
      for (int z = 0; z < 3 && s_isdigit(*chr); z++)
        cp = cp * 10 + int((*chr++) - '0');
      return cp;
    }
    else
      return utf8::parse(utf8::transform(&chr, NULL));
  }
}
static uint32 getchar(uint8_const_ptr& chr, uint32* table = NULL)
{
  if (*chr == '\\')
    return unescape(++chr);
  return utf8::parse(utf8::transform(&chr, table));
}

int CharacterClass::rangecomp(Range const& a, Range const& b)
{
  return int(a.begin - b.begin);
}
void CharacterClass::sort()
{
  data.sort(rangecomp);
  int shift = 0;
  for (int i = 0; i < data.length(); i++)
  {
    if (data[i].end < data[i].begin || (i - shift > 0 && data[i].end <= data[i - shift - 1].end))
      shift++;
    else if (i - shift > 0 && data[i].begin <= data[i - shift - 1].end + 1)
    {
      data[i - shift - 1].end = data[i].end;
      shift++;
    }
    else if (shift)
      data[i - shift] = data[i];
  }
  data.resize(data.length() - shift);
}

void CharacterClass::addClass(CharacterClass const& cls)
{
  funcs.append(cls.funcs);
  if (cls.invert)
  {
    if (cls.data.length() == 0)
      addRange(0, 0xFFFFFFFF);
    else
    {
      if (cls.data[0].begin > 0)
        addRange(0, cls.data[0].begin - 1);
      for (int i = 1; i < cls.data.length(); i++)
        addRange(cls.data[i - 1].end + 1, cls.data[i].begin - 1);
      if (cls.data[cls.data.length() - 1].end < 0xFFFFFFFF)
        addRange(cls.data[cls.data.length() - 1].end + 1, 0xFFFFFFFF);
    }
  }
  else
    data.append(cls.data);
}
uint8_const_ptr CharacterClass::init(char const* src, int flags)
{
  invert = (*src == '^');
  uint8_const_ptr pos = (uint8_const_ptr) src;
  invert = (*src == '^');
  if (invert)
    src++;
  uint32* table = ((flags & Prog::CaseInsensitive) ? utf8::tf_lower : NULL);
  while (*pos && (pos == (uint8_const_ptr) src || *pos != ']'))
  {
    uint32 cp = -1;
    if (*pos == '\\')
    {
      pos++;
      CharacterClass* cls = getDefault(*pos, flags);
      if (cls)
      {
        pos++;
        addClass(*cls);
      }
      else
      {
        cp = unescape(pos);
        if (flags & Prog::CaseInsensitive)
          cp = towlower(cp);
      }
    }
    else
      cp = utf8::parse(utf8::transform(&pos, table));
    if (cp != -1)
    {
      if (*pos == '-' && pos[1] && pos[1] != ']')
      {
        pos++;
        addRange(cp, getchar(pos, table));
      }
      else
        addRange(cp, cp);
    }
  }
  sort();
  return pos;
}
bool CharacterClass::match(uint32 c) const
{
  int left = 0;
  int right = data.length() - 1;
  while (left <= right)
  {
    int mid = (left + right) / 2;
    if (c >= data[mid].begin && c <= data[mid].end)
      return !invert;
    if (c < data[mid].begin)
      right = mid - 1;
    else
      left = mid + 1;
  }
  for (int i = 0; i < funcs.length(); i++)
    if (funcs[i](c))
      return !invert;
  return invert;
}

static CharacterClass uClsNormal[] = {
  CharacterClass("a-zA-Z0-9_"),
  CharacterClass("^a-zA-Z0-9_"),
  CharacterClass("0-9"),
  CharacterClass("a-fA-F0-9"),
  CharacterClass("^0-9"),
  CharacterClass(" \t\n\r\f\v"),
  CharacterClass("^ \t\n\r\f\v"),
  CharacterClass("^\n"),
  CharacterClass("^"),
};
static bool u_word(uint32 cp)
{
  return cp == '_' || (cp >= '0' && cp <= '9') || iswalnum(cp);
}
static bool u_notword(uint32 cp)
{
  return !(cp == '_' || (cp >= '0' && cp <= '9') || iswalnum(cp));
}
static bool u_space(uint32 cp)
{
  return iswspace(cp);
}
static bool u_notspace(uint32 cp)
{
  return !iswspace(cp);
}
static CharacterClass uClsUnicode[] = {
  CharacterClass(u_word),
  CharacterClass(u_notword),
  CharacterClass(u_space),
  CharacterClass(u_notspace),
};

CharacterClass* CharacterClass::getDefault(char ch, int flags)
{
  switch (ch)
  {
  case 'w': return (flags & Prog::Unicode ? &uClsUnicode[0] : &uClsNormal[0]);
  case 'W': return (flags & Prog::Unicode ? &uClsUnicode[1] : &uClsNormal[1]);
  case 'd': return &uClsNormal[2];
  case 'x': return &uClsNormal[3];
  case 'D': return &uClsNormal[4];
  case 's': return (flags & Prog::Unicode ? &uClsUnicode[2] : &uClsNormal[5]);
  case 'S': return (flags & Prog::Unicode ? &uClsUnicode[3] : &uClsNormal[6]);
  case '.': return (flags & Prog::DotAll ? &uClsNormal[8] : &uClsNormal[7]);
  default: return NULL;
  }
}

/////////////////////////////////////

struct State
{
  enum {
    START = 1,
    RBRA,
    RBRANC,
    LBRA,
    LBRANC,
    OR,
    CAT,
    STAR,
    PLUS,
    QUEST,
    NOP,
    OPERAND,
    BOL,
    EOL,
    CHAR,
    CCLASS,
    END
  };
  int type;
  union
  {
    CharacterClass const* mask;
    uint32 chr;
    int subid;
    State* left;
  };
  union
  {
    State* right;
    State* next;
  };
  int list;
};
struct Operand
{
  State* first;
  State* last;
};
struct Operator
{
  int type;
  int subid;
};
#define MAXSTACK    32
struct Compiler
{
  State* states;
  int maxStates;
  int numStates;

  Operand andstack[MAXSTACK];
  int andsize;
  Operator atorstack[MAXSTACK];
  int atorsize;

  bool lastand;
  int brackets;
  int cursub;

  int optsize;

  void init(char const* expr, int len);
  State* operand(int type);
  void pushand(State* first, State* last);
  void pushator(int type);
  void evaluntil(int pri);
  int optimize(State* state);
  void floatstart();
};
void Compiler::init(char const* expr, int length)
{
  maxStates = length * 6 + 6;
  states = new State[maxStates];
  memset(states, 0, sizeof(State) * maxStates);
  numStates = 0;

  andsize = 0;
  atorstack[0].type = 0;
  atorstack[0].subid = 0;
  atorsize = 1;
  lastand = false;
  brackets = 0;
  cursub = 0;

  optsize = 0;
}
State* Compiler::operand(int type)
{
  if (lastand)
    pushator(State::CAT);
  State* s = &states[numStates++];
  s->type = type;
  s->mask = NULL;
  s->next = NULL;
  pushand(s, s);
  lastand = true;
  return s;
}
void Compiler::pushand(State* first, State* last)
{
  andstack[andsize].first = first;
  andstack[andsize].last = last;
  andsize++;
}
void Compiler::pushator(int type)
{
  if (type == State::RBRA || type == State::RBRANC)
    brackets--;
  if (type == State::LBRA || type == State::LBRANC)
  {
    if (type == State::LBRA)
      cursub++;
    brackets++;
    if (lastand)
      pushator(State::CAT);
  }
  else
    evaluntil(type);
  if (type != State::RBRA && type != State::RBRANC)
  {
    atorstack[atorsize].type = type;
    atorstack[atorsize].subid = cursub;
    atorsize++;
  }
  lastand = (type == State::STAR || type == State::PLUS ||
    type == State::QUEST || type == State::RBRA || type == State::RBRANC);
}
void Compiler::evaluntil(int pri)
{
  State* s1;
  State* s2;
  while (pri == State::RBRA || pri == State::RBRANC || atorstack[atorsize - 1].type >= pri)
  {
    atorsize--;
    switch (atorstack[atorsize].type)
    {
    case State::LBRA:
      s1 = &states[numStates++];
      s2 = &states[numStates++];
      s1->type = State::LBRA;
      s1->subid = atorstack[atorsize].subid;
      s2->type = State::RBRA;
      s2->subid = atorstack[atorsize].subid;

      s1->next = andstack[andsize - 1].first;
      andstack[andsize - 1].first = s1;
      andstack[andsize - 1].last->next = s2;
      andstack[andsize - 1].last = s2;
      s2->next = NULL;
      return;
    case State::LBRANC:
      return;
    case State::OR:
      andsize--;
      s1 = &states[numStates++];
      s2 = &states[numStates++];
      s1->type = State::OR;
      s1->left = andstack[andsize - 1].first;
      s1->right = andstack[andsize].first;
      s2->type = State::NOP;
      s2->mask = NULL;
      s2->next = NULL;
      andstack[andsize - 1].last->next = s2;
      andstack[andsize].last->next = s2;
      andstack[andsize - 1].first = s1;
      andstack[andsize - 1].last = s2;
      break;
    case State::CAT:
      andsize--;
      andstack[andsize - 1].last->next = andstack[andsize].first;
      andstack[andsize - 1].last = andstack[andsize].last;
      break;
    case State::STAR:
      s1 = &states[numStates++];
      s1->type = State::OR;
      s1->left = andstack[andsize - 1].first;
      s1->right = NULL;
      andstack[andsize - 1].last->next = s1;
      andstack[andsize - 1].first = s1;
      andstack[andsize - 1].last = s1;
      break;
    case State::PLUS:
      s1 = &states[numStates++];
      s1->type = State::OR;
      s1->left = andstack[andsize - 1].first;
      s1->right = NULL;
      andstack[andsize - 1].last->next = s1;
      andstack[andsize - 1].last = s1;
      break;
    case State::QUEST:
      s1 = &states[numStates++];
      s2 = &states[numStates++];
      s1->type = State::OR;
      s1->left = andstack[andsize - 1].first;
      s1->right = s2;
      s2->type = State::NOP;
      s2->mask = NULL;
      s2->next = NULL;
      andstack[andsize - 1].last->next = s2;
      andstack[andsize - 1].first = s1;
      andstack[andsize - 1].last = s2;
      break;
    }
  }
}
int Compiler::optimize(State* state)
{
  if (state->list >= 0)
    return state->list;
  if (state->type == State::NOP)
    state->list = optimize(state->next);
  else
  {
    state->list = optsize++;
    if (state->next)
      optimize(state->next);
    if (state->type == State::OR)
      optimize(state->left);
  }
  return state->list;
}

struct Thread
{
  State* state;
  Match match;
};

Prog::Prog(char const* expr, int length, uint32 f)
{
  flags = f;
  Compiler comp;
  if (length < 0)
    length = strlen(expr);
  comp.init(expr, length);

  uint32* ut_table = (flags & CaseInsensitive ? utf8::tf_lower : NULL);

  uint8_const_ptr pos = (uint8_const_ptr) expr;
  uint8_const_ptr end = pos + length;
  while (true)
  {
    int type = State::CHAR;
    CharacterClass const* mask = NULL;
    uint32 cp = -1;
    switch (*pos)
    {
    case '\\':
      pos++;
      if (mask = CharacterClass::getDefault(*pos, flags))
        type = State::CCLASS;
      else
      {
        cp = unescape(pos);
        if (flags & CaseInsensitive)
          cp = towlower(cp);
        pos--;
      }
      break;
    case 0:
      if (pos == end)
        type = State::END;
      break;
    case '*':
      type = State::STAR;
      break;
    case '+':
      type = State::PLUS;
      break;
    case '?':
      type = State::QUEST;
      break;
    case '|':
      type = State::OR;
      break;
    case '(':
      type = State::LBRA;
      break;
    case ')':
      type = State::RBRA;
      break;
    case '{':
      type = State::LBRANC;
      break;
    case '}':
      type = State::RBRANC;
      break;
    case '^':
      type = State::BOL;
      break;
    case '$':
      type = State::EOL;
      break;
    case '.':
      type = State::CCLASS;
      mask = CharacterClass::getDefault('.', flags);
      break;
    case '[':
      {
        type = State::CCLASS;
        CharacterClass* cls = new CharacterClass();
        masks.push(cls);
        pos = cls->init((char*) pos + 1, flags);
        mask = cls;
      }
      break;
    }
    if (cp == -1)
      cp = utf8::parse(utf8::transform(&pos, ut_table));
    else
      pos++;
    if (type < State::OPERAND)
      comp.pushator(type);
    else
    {
      State* s = comp.operand(type);
      if (type == State::CHAR)
        s->chr = cp;
      else if (type == State::CCLASS)
        s->mask = mask;
    }
    if (type == State::END)
      break;
  }
  comp.evaluntil(State::START);

  for (int i = 0; i < comp.numStates; i++)
    comp.states[i].list = -1;
  int startpos = comp.optimize(comp.andstack[0].first);
  states = new State[comp.optsize];
  for (int i = 0; i < comp.numStates; i++)
  {
    int p = comp.states[i].list;
    if (p >= 0 && comp.states[i].type != State::NOP)
    {
      states[p].type = comp.states[i].type;
      states[p].next = comp.states[i].next ? &states[comp.states[i].next->list] : NULL;
      if (states[p].type == State::CHAR)
        states[p].chr = comp.states[i].chr;
      else if (states[p].type == State::CCLASS)
        states[p].mask = comp.states[i].mask;
      else if (states[p].type == State::OR)
        states[p].left = comp.states[i].left ? &states[comp.states[i].left->list] : NULL;
      else
        states[p].subid = comp.states[i].subid;
    }
  }
  delete[] comp.states;
  numStates = comp.optsize;
  start = &states[startpos];

  numCaptures = comp.cursub;
  maxThreads = 32;
  threads = new Thread[maxThreads * 2];
}
Prog::~Prog()
{
  delete[] states;
  for (int i = 0; i < masks.length(); i++)
    delete masks[i];
  delete[] threads;
}
void Prog::addthread(State* state, Match const& match)
{
  if (state->list < 0)
  {
    if (numThreads[1 - cur] >= maxThreads)
    {
      Thread* newthreads = new Thread[maxThreads * 4];
      memcpy(newthreads, threads, sizeof(Thread) * numThreads[0]);
      memcpy(newthreads + maxThreads * 2, threads + maxThreads, sizeof(Thread) * numThreads[1]);
      delete[] threads;
      threads = newthreads;
      maxThreads *= 2;
    }
    Thread* thread = &threads[(1 - cur) * maxThreads + numThreads[1 - cur]];
    state->list = numThreads[1 - cur];
    numThreads[1 - cur]++;
    thread->state = state;
    memcpy(&thread->match, &match, sizeof match);
  }
}
void Prog::advance(State* state, Match const& match, uint32 cp, char const* ref)
{
  if (state->type == State::OR)
  {
    advance(state->left, match, cp, ref);
    advance(state->right, match, cp, ref);
  }
  else if (state->type == State::LBRA)
  {
    Match m2;
    memcpy(&m2, &match, sizeof m2);
    m2.start[state->subid] = ref;
    advance(state->next, m2, cp, ref);
  }
  else if (state->type == State::RBRA)
  {
    Match m2;
    memcpy(&m2, &match, sizeof m2);
    m2.end[state->subid] = ref;
    advance(state->next, m2, cp, ref);
  }
  else if (state->type == State::BOL)
  {
    if (flags & MultiLine)
    {
      if (ref == matchText || ref[-1] == '\n')
        advance(state->next, match, 0xFFFFFFFF, ref);
    }
    else
    {
      if (ref == matchText)
        advance(state->next, match, 0xFFFFFFFF, ref);
    }
  }
  else if (state->type == State::EOL)
  {
    if (flags & MultiLine)
    {
      if (*ref == 0 || *ref == '\r' || *ref == '\n')
        advance(state->next, match, 0xFFFFFFFF, ref);
    }
    else
    {
      if (*ref == 0)
        advance(state->next, match, 0xFFFFFFFF, ref);
    }
  }
  else
  {
    if (cp == 0xFFFFFFFF)
      addthread(state, match);
    else if ((state->type == State::CHAR && cp == state->chr) ||
             (state->type == State::CCLASS && state->mask->match(cp)))
      advance(state->next, match, 0xFFFFFFFF, (char*) utf8::next((uint8_const_ptr) ref));
  }
}
int Prog::run(char const* text, int length, bool exact,
                      bool (*callback) (Match const& match, void* arg), void* arg)
{
  cur = 0;
  numThreads[0] = 0;
  numThreads[1] = 0;
  int pos = 0;
  if (length < 0)
    length = strlen(text);
  matchText = text;
  int count = 0;
  uint32* ut_table = (flags & CaseInsensitive ? utf8::tf_lower : NULL);

  while (true)
  {
    for (int i = 0; i < numStates; i++)
    {
      if (pos > 0 && states[i].type == State::END && states[i].list >= 0 &&
        (!exact || pos == length))
      {
        Thread* thread = &threads[cur * maxThreads + states[i].list];
        thread->match.end[0] = text + pos;
        count++;
        if (callback)
        {
          if (!callback(thread->match, arg))
            return count;
        }
      }
      states[i].list = -1;
    }
    numThreads[1 - cur] = 0;
    uint8_const_ptr next = (uint8_const_ptr) (text + pos);
    uint32 cp = utf8::parse(utf8::transform(&next, ut_table));
    if (cp == '\r' && *next == '\n')
      next++;
    for (int i = 0; i < numThreads[cur]; i++)
      advance(threads[cur * maxThreads + i].state, threads[cur * maxThreads + i].match, cp, text + pos);
    if (pos == 0 || !exact)
    {
      Match match;
      memset(&match, 0, sizeof match);
      match.start[0] = text + pos;
      advance(start, match, cp, text + pos);
    }
    cur = 1 - cur;
    if (pos >= length)
      break;
    pos = (char*) next - text;
  }
  for (int i = 0; i < numStates; i++)
  {
    if (states[i].type == State::END && states[i].list >= 0)
    {
      Thread* thread = &threads[cur * maxThreads + states[i].list];
      thread->match.end[0] = text + pos;
      count++;
      if (callback)
      {
        if (!callback(thread->match, arg))
          return count;
      }
    }
  }
  return count;
}

static bool matcher(Match const& match, void* arg)
{
  if (arg)
  {
    Array<String>* sub = (Array<String>*) arg;
    sub->clear();
    for (int i = 0; i < sizeof(match.start) / sizeof(match.start[0]) && match.start[i]; i++)
      sub->push(String(match.start[i], match.end[i] - match.start[i]));
  }
  return true;
}
bool Prog::match(char const* text, Array<String>* sub)
{
  int res = run(text, -1, true, matcher, sub);
  if (res)
  {
    if (sub)
    {
      while (sub->length() <= numCaptures)
        sub->push("");
    }
    return true;
  }
  else
    return false;
}
static bool finder(Match const& match, void* arg)
{
  memcpy(arg, &match, sizeof match);
  return false;
}
int Prog::find(char const* text, int start, Array<String>* sub)
{
  Match match;
  if (run(text + start, -1, false, finder, &match))
  {
    if (sub)
    {
      sub->clear();
      for (int i = 0; i < sizeof(match.start) / sizeof(match.start[0]) && match.start[i]; i++)
        sub->push(String(match.start[i], match.end[i] - match.start[i]));
      while (sub->length() <= numCaptures)
        sub->push("");
    }
    return match.start[0] - text;
  }
  else
    return -1;
}
struct ReplaceStruct
{
  String result;
  char const* text;
  char const* with;
  int last_start;
  int last_end;
  Match last_match;
};
static void addreplace(String& result, char const* with, Match const& match)
{
  for (int i = 0; with[i]; i++)
  {
    if (with[i] == '\\')
    {
      i++;
      if (with[i] >= '0' && with[i] <= '9')
      {
        int m = int(with[i] - '0');
        if (match.start[m] && match.end[m])
          for (char const* s = match.start[m]; s < match.end[m]; s++)
            result += *s;
      }
      else
        result += with[i];
    }
    else
      result += with[i];
  }
}
static bool replacer(Match const& match, void* arg)
{
  ReplaceStruct& rs = * (ReplaceStruct*) arg;
  int mstart = match.start[0] - rs.text;
  int mend = match.end[0] - rs.text;
  if (mstart == rs.last_start && mend > rs.last_end)
  {
    rs.last_end = mend;
    memcpy(&rs.last_match, &match, sizeof match);
  }
  else if (mstart >= rs.last_end)
  {
    for (int i = rs.last_end; i < mstart; i++)
      rs.result += rs.text[i];
    addreplace(rs.result, rs.with, rs.last_match);
    rs.last_start = mstart;
    rs.last_end = mend;
    memcpy(&rs.last_match, &match, sizeof match);
  }
  return true;
}
String Prog::replace(char const* text, char const* with)
{
  ReplaceStruct rs;
  rs.text = text;
  rs.with = with;
  rs.last_start = 0;
  rs.last_end = 0;
  run(text, -1, false, replacer, &rs);
  if (rs.last_end)
    addreplace(rs.result, with, rs.last_match);
  rs.result += (text + rs.last_end);
  return rs.result;
}

}

//String RegExp::dump()
//{
//  String result;
//  for (int i = 0; i < prog->numStates; i++)
//  {
//    State* s = &prog->states[i];
//    if (s == prog->start)
//      result += '*';
//    result.printf("%d: ", i);
//    if (s->type == State::CHAR)
//      result.printf("%c", s->chr);
//    else if (s->type == State::CCLASS)
//      result.printf("[class]");
//    else if (s->type == State::END)
//      result.printf("[end]");
//    else if (s->type == State::RBRA)
//      result.printf("RBRA(%d)", s->subid);
//    else if (s->type == State::LBRA)
//      result.printf("LBRA(%d)", s->subid);
//    else if (s->type == State::OR)
//      result.printf("OR");
//    else if (s->type == State::BOL)
//      result.printf("BOL");
//    else if (s->type == State::EOL)
//      result.printf("EOL");
//    if (s->type == State::OR)
//      result.printf(" -> %d, %d\n", s->left ? s->left - prog->states : -1, s->right ? s->right - prog->states : -1);
//    else
//      result.printf(" -> %d\n", s->next ? s->next - prog->states : -1);
//  }
//  return result;
//}
//
//static bool addmatch(Match const& match, void* arg)
//{
//  String& result = * (String*) arg;
//  result += "Match: ";
//  for (char const* c = match.start[0]; c < match.end[0]; c++)
//    result += *c;
//  result += "\n";
//  for (int i = 1; i < sizeof match.start / sizeof match.start[0]; i++)
//  {
//    if (match.start[i] && match.end[i])
//    {
//      result += "  Capture: ";
//      for (char const* c = match.start[i]; c < match.end[i]; c++)
//        result += *c;
//      result += "\n";
//    }
//  }
//  return true;
//}
//String RegExp::find(char const* text, bool exact)
//{
//  String result = "";
//  int found = prog->run(text, exact, addmatch, &result);
//  result.printf("Found %d matches\n", found);
//  return result;
//}
