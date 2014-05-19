#include "luaeval.h"
#include "base/string.h"
#include "base/dictionary.h"
#include <math.h>

enum RESERVED {
  TK_AND = 257, TK_BREAK,
  TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR, TK_FUNCTION,
  TK_GOTO, TK_IF, TK_IN, TK_LOCAL, TK_NIL, TK_NOT, TK_OR, TK_REPEAT,
  TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE,
  TK_CONCAT, TK_DOTS, TK_EQ, TK_GE, TK_LE, TK_NE, TK_DBCOLON, TK_EOS,
  TK_NUMBER, TK_NAME, TK_STRING
};

namespace luae
{

enum {lexEnd, lexLit, lexName, lexSym, lexTable};
enum {litNil, litFalse, litTrue, litNumber, litString, litLongString};
enum {symBracket, symBinop, symUnop};
struct Lexem
{
  int type;
  int subtype;
  String value;
  union
  {
    int op;
    double dvalue;
  };
};
inline int xtoi(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0;
}
char const* parse(char const* str, Lexem& lex)
{
  while (s_isspace(*str))
    str++;
  lex.op = 0;
  switch (*str)
  {
  case 0:
    lex.type = lexEnd;
    lex.op = 0;
    return str;
  case '[':
    if (str[1] == '[' || str[1] == '=')
    {
      str++;
      int level = 0;
      while (*str == '=')
        level++, str++;
      if (*str != '[')
        return NULL;
      char const* start = ++str;
      while (*str)
      {
        if (*str == ']')
        {
          char const* end = str++;
          int count = 0;
          while (*str == '=' && count < level)
            count++, str++;
          if (*str == ']' && count == level)
          {
            lex.type = lexLit;
            lex.subtype = litLongString;
            lex.value = String(start, end - start);
            return str + 1;
          }
        }
        else
          str++;
      }
      return NULL;
    }
  case ']':
  case '(':
  case ')':
    lex.type = lexSym;
    lex.subtype = symBracket;
    lex.value = String(*str);
    lex.op = *str;
    return str + 1;
  case '{':
  case '}':
  case ',':
    lex.type = lexTable;
    lex.value = String(*str);
    lex.op = *str;
    return str + 1;
  case '+':
  case '-':
  case '*':
  case '/':
  case '^':
  case '%':
    lex.type = lexSym;
    lex.subtype = symBinop;
    lex.value = String(*str);
    lex.op = *str;
    return str + 1;
  case '<':
  case '>':
  case '=':
  case '~':
    lex.type = lexSym;
    lex.subtype = symBinop;
    lex.value = String(*str++);
    lex.op = *str;
    if (*str == '=')
    {
      lex.value += *str++;
      if (*str == '<') lex.op = TK_LE;
      else if (*str == '>') lex.op = TK_GE;
      else if (*str == '=') lex.op = TK_EQ;
      else if (*str == '~') lex.op = TK_NE;
    }
    else if (lex.value == "=")
      lex.type = lexTable;
    else if (lex.value == "~")
      return NULL;
    return str;
  case '#':
    lex.type = lexSym;
    lex.subtype = symUnop;
    lex.value = "#";
    lex.op = '#';
    return str + 1;

  case '\'':
  case '"':
    {
      char delim = *str++;
      char const* start = str;
      while (*str && *str != delim)
      {
        if (*str == '\\')
          str++;
        str++;
      }
      if (*str != delim)
        return NULL;
      lex.type = lexLit;
      lex.subtype = litString;
      lex.value = String(start, str - start);
    }
    return str + 1;

  case '.':
    if (str[1] == '.')
    {
      if (str[2] == '.')
        return NULL; //TODO: vararg
      lex.type = lexSym;
      lex.subtype = symBinop;
      lex.value = "..";
      lex.op = TK_CONCAT;
      return str + 2;
    }
    else if (!s_isdigit(str[1]))
    {
      lex.type = lexSym;
      lex.subtype = symBracket;
      lex.value = ".";
      lex.op = '.';
      return str + 1;
    }
  default:
    if (*str == '.' || s_isdigit(*str))
    {
      char const* start = str;
      lex.dvalue = 0;
      int dexp = 0;
      bool ishex = false;
      bool expo = false;
      if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
      {
        ishex = true;
        str += 2;
        while (s_isxdigit(*str))
          lex.dvalue = lex.dvalue * 16 + xtoi(*str++);
        if (*str == '.')
        {
          str++;
          while (s_isxdigit(*str))
            dexp -= 4, lex.dvalue = lex.dvalue * 16 + xtoi(*str++);
        }
        if (*str == 'p' || *str == 'P')
          expo = true;
      }
      else
      {
        while (s_isdigit(*str))
          lex.dvalue = lex.dvalue * 10 + int((*str++) - '0');
        if (*str == '.')
        {
          str++;
          while (s_isdigit(*str))
            dexp--, lex.dvalue = lex.dvalue * 10 + int((*str++) - '0');
        }
        if (*str == 'e' || *str == 'E')
          expo = true;
      }
      if (expo)
      {
        str++;
        int sgn = 1;
        if ((str[0] == '+' || str[0] == '-') && s_isdigit(str[1]))
          sgn = (*str++ == '+' ? 1 : -1);
        int lexp = 0;
        while (s_isdigit(*str))
          lexp = lexp * 10 + int((*str++) - '0');
        dexp += lexp * sgn;
      }
      if (dexp)
        lex.dvalue *= (ishex ? 2.0 : 10.0, dexp);
      lex.type = lexLit;
      lex.subtype = litNumber;
      lex.value = String(start, str - start);
      return str;
    }
    else if (*str == '_' || s_isalpha(*str))
    {
      char const* start = str;
      while (*str == '_' || s_isalnum(*str))
        str++;
      lex.value = String(start, str - start);
      if (lex.value == "nil")
        lex.type = lexLit, lex.subtype = litNil;
      else if (lex.value == "false")
        lex.type = lexLit, lex.subtype = litFalse;
      else if (lex.value == "true")
        lex.type = lexLit, lex.subtype = litTrue;
      else if (lex.value == "and")
        lex.type = lexSym, lex.subtype = symBinop;
      else if (lex.value == "or")
        lex.type = lexSym, lex.subtype = symBinop;
      else if (lex.value == "not")
        lex.type = lexSym, lex.subtype = symUnop;
      else
        lex.type = lexName;
      return str;
    }
    return NULL;
  }
}

void pushlit(lua_State* L, Lexem const& lex)
{
  switch (lex.subtype)
  {
  case litNil:
    lua_pushnil(L);
    break;
  case litFalse:
    lua_pushboolean(L, 0);
    break;
  case litTrue:
    lua_pushboolean(L, 1);
    break;
  case litNumber:
    lua_pushnumber(L, lex.dvalue);
    break;
  case litString:
    {
      luaL_Buffer b;
      luaL_buffinit(L, &b);
      char const* ptr = lex.value.c_str();
      while (*ptr)
      {
        if (*ptr == '\\')
        {
          ptr++;
          switch (*ptr)
          {
          case 'a':
            ptr++, luaL_addchar(&b, '\a');
            break;
          case 'b':
            ptr++, luaL_addchar(&b, '\b');
            break;
          case 'f':
            ptr++, luaL_addchar(&b, '\f');
            break;
          case 'n':
            ptr++, luaL_addchar(&b, '\n');
            break;
          case 'r':
            ptr++, luaL_addchar(&b, '\r');
            break;
          case 't':
            ptr++, luaL_addchar(&b, '\t');
            break;
          case 'v':
            ptr++, luaL_addchar(&b, '\v');
            break;
          default:
            if (s_isdigit(*ptr))
            {
              int cnt = 0;
              int code = 0;
              while (s_isdigit(*ptr) && cnt < 3)
                cnt++, code = code * 10 + int((*ptr++) - '0');
              luaL_addchar(&b, code);
            }
            else
              luaL_addchar(&b, *ptr++);
          }
        }
        else
          luaL_addchar(&b, *ptr++);
      }
      luaL_pushresult(&b);
    }
    break;
  case litLongString:
    lua_pushstring(L, lex.value);
    break;
  default:
    lua_pushnil(L);
  }
}

enum {
  opBegin, opEnd,
  opOpen, opClose,
  opSopen, opSclose,

  opOr, opAnd,
  opEq, opNe,
  opLt, opLe,
  opGt, opGe,
  opConcat,
  opPlus, opMinus,
  opMul, opDiv, opMod,
  opNeg, opNot, opLen,
  opPow,

  opCount,
  opBrackets = opSclose
};
inline int getOpRaw(int code, int val)
{
  switch(code)
  {
  case 0:         return opEnd;
  case '}':       return opEnd;
  case ',':       return opEnd;
  case '(':       return opOpen;
  case ')':       return opClose;
  case '[':       return opSopen;
  case ']':       return opSclose;
  case TK_OR:     return opOr;
  case TK_AND:    return opAnd;
  case TK_EQ:     return opEq;
  case TK_NE:     return opNe;
  case '<':       return opLt;
  case TK_LE:     return opLe;
  case '>':       return opGt;
  case TK_GE:     return opGe;
  case TK_CONCAT: return opConcat;
  case '+':       return opPlus;
  case '-':       return (val ? opNeg : opMinus);
  case '*':       return opMul;
  case '/':       return opDiv;
  case '%':       return opMod;
  case TK_NOT:    return opNot;
  case '#':       return opLen;
  case '^':       return opPow;
  default:        return -1;
  }
}
struct
{
  int left;
  int right;
} lprio[opCount] = {
  {0, 0}, {0, 0},             // {   }
  {13, 0}, {0, 13},           // (   )
  {13, 0}, {0, 13},           // [   ]
  {2, 2}, {3, 3},             // or  and
  {4, 4}, {4, 4},             // ==  ~=
  {4, 4}, {4, 4},             // <   <=
  {4, 4}, {4, 4},             // >   >=
  {6, 5},                     // ..
  {7, 7}, {7, 7},             // +   -
  {8, 8}, {8, 8}, {8, 8},     // *   /   %
  {10, 9}, {10, 9}, {10, 9},  // -x  not #
  {10, 9},                    // ^
};
struct
{
  int left;
  int right;
} lval[opCount] = {
  {0, 1}, {0, 0},             // {   }
  {1, 1}, {0, 0},             // (   )
  {0, 1}, {0, 0},             // [   ]
  {0, 1}, {0, 1},             // or  and
  {0, 1}, {0, 1},             // ==  ~=
  {0, 1}, {0, 1},             // <   <=
  {0, 1}, {0, 1},             // >   >=
  {0, 1},                     // ..
  {0, 1}, {0, 1},             // +   -
  {0, 1}, {0, 1}, {0, 1},     // *   /   %
  {1, 1}, {1, 1}, {1, 1},     // -x  not #
  {0, 1},                     // ^
};
int getOp(int code, int val)
{
  int op = getOpRaw(code, val);
  if (op >= 0 && val != lval[op].left)
    return -1;
  return op;
}

inline bool lbinary(lua_State* L)
{
  lua_replace(L, -3);
  lua_pop(L, 1);
  return true;
}
inline bool lunary(lua_State* L)
{
  lua_replace(L, -2);
  return true;
}
double larith(int op, double a, double b)
{
  switch (op)
  {
  case opPlus:    return a + b;
  case opMinus:   return a - b;
  case opMul:     return a * b;
  case opDiv:     return a / b;
  case opMod:     return a - floor(a / b) * b;
  case opPow:     return pow(a, b);
  default:        return 0;
  }
}
bool lexec(lua_State* L, int op)
{
  switch (op)
  {
  case opSopen:
    if (!lua_istable(L, -2)) return false;
    lua_rawget(L, -2);
    lua_remove(L, -2);
    return true;
  case opOr:
    lua_pushboolean(L, lua_toboolean(L, -2) || lua_toboolean(L, -1));
    return lbinary(L);
  case opAnd:
    lua_pushboolean(L, lua_toboolean(L, -2) && lua_toboolean(L, -1));
    return lbinary(L);
  case opEq:
    lua_pushboolean(L, lua_rawequal(L, -2, -1));
    return lbinary(L);
  case opNe:
    lua_pushboolean(L, !lua_rawequal(L, -2, -1));
    return lbinary(L);
  case opLt:
  case opLe:
  case opGt:
  case opGe:
    {
      double diff = 0;
      int ta = lua_type(L, -2);
      if (ta != lua_type(L, -1)) return false;
      if (ta == LUA_TNUMBER)
        diff = lua_tonumber(L, -2) - lua_tonumber(L, -1);
      else if (ta == LUA_TSTRING)
        diff = (double) strcmp(lua_tostring(L, -2), lua_tostring(L, -1));
      else
        return false;
      if (op == opLt) lua_pushboolean(L, diff < 0);
      else if (op == opLe) lua_pushboolean(L, diff <= 0);
      else if (op == opGt) lua_pushboolean(L, diff > 0);
      else if (op == opGe) lua_pushboolean(L, diff >= 0);
    }
    return lbinary(L);
  case opPlus:
  case opMinus:
  case opMul:
  case opDiv:
  case opMod:
  case opPow:
    if (lua_type(L, -2) == LUA_TNUMBER && lua_type(L, -1) == LUA_TNUMBER)
      lua_pushnumber(L, larith(op, lua_tonumber(L, -2), lua_tonumber(L, -2)));
    else
      return false;
    return lbinary(L);
  case opConcat:
    if (lua_isstring(L, -2) && lua_isstring(L, -1))
      lua_concat(L, 2);
    else
      return false;
    return true;

  case opNeg:
    if (lua_type(L, -1) == LUA_TNUMBER)
      lua_pushnumber(L, -lua_tonumber(L, -1));
    else
      return false;
    return lunary(L);
  case opNot:
    lua_pushboolean(L, !lua_toboolean(L, -1));
    return lunary(L);
  case opLen:
    lua_pushinteger(L, lua_rawlen(L, -1));
    return lunary(L);
  }
  return false;
}

struct RunContext
{
  lua_State* L;
  lua_Debug ar;
  Dictionary<int> locals;
  int i;
  Array<Lexem> expr;
  bool lvalue;

  int depth;

  int unres(int& lres);
  int dores(int& lres, int op);

  int rawdoeval();
  int doeval();
};

int RunContext::unres(int& lres)
{
  if (lres > 0)
    lua_getlocal(L, &ar, lres);
  else if (lres == eTableKey && !lexec(L, opSopen))
    return eFail;
  else if (lres == eMetaTable)
  {
    if (!lua_istable(L, -1))
      return eFail;
    lua_getmetatable(L, -1);
    lua_remove(L, -2);
  }
  lres = eValue;
  return 0;
}
int RunContext::dores(int& lres, int op)
{
  if (unres(lres))
    return eFail;
  lres = op;
  if (!lvalue || depth > 1)
    if (unres(lres))
      return eFail;
  return 0;
}

int RunContext::rawdoeval()
{
  int top = lua_gettop(L);

  int otop = lua_gettop(L);
  int lres = eValue;
  int val = 1;
  Array<int> ops;
  ops.push(opBegin);
  for (; i < expr.length(); i++)
  {
    if (expr[i].type == lexTable && expr[i].op == '{')
    {
      if (val == 0)
        return eFail;
      val = 0;
      i++;
      if (unres(lres))
        return eFail;
      int order = 1;
      lua_newtable(L);
      while (true)
      {
        if (expr[i].op == '[')
        {
          i++;
          if (doeval() != eValue)
            return eFail;
          if (expr[i].op != ']')
            return eFail;
          i++;
          if (expr[i].op != '=')
            return eFail;
          i++;
        }
        else if (expr[i].type == lexName && expr[i + 1].op == '=')
        {
          lua_pushstring(L, expr[i].value);
          i += 2;
        }
        else
          lua_pushinteger(L, order++);
        if (doeval() != eValue)
          return eFail;
        lua_rawset(L, -3);
        if (expr[i].op == '}')
          break;
        if (expr[i].op != ',')
          return eFail;
        i++;
      }
    }
    else if (expr[i].type == lexSym && expr[i].op == '.')
    {
      i++;
      if (expr[i].type != lexName)
        return eFail;
      if (unres(lres))
        return eFail;
      lua_pushstring(L, expr[i].value);
      if (dores(lres, eTableKey))
        return eFail;
    }
    else if (expr[i].type == lexEnd || expr[i].type == lexSym || expr[i].type == lexTable)
    {
      int op = getOp(expr[i].op, val);
      if (op < 0) return eFail;
      val = lval[op].right;

      while (ops.length() > 0 && lprio[ops[ops.length() - 1]].right >= lprio[op].left)
      {
        int oop = ops[ops.length() - 1];
        ops.pop();
        if (oop <= opBrackets)
        {
          if (op != oop + 1)
            return oop == opBegin ? lres : eFail;
          if (oop != opSopen)
            break;
        }
        if (unres(lres))
          return eFail;
        if (oop == opSopen)
        {
          if (dores(lres, eTableKey))
            return eFail;
          break;
        }
        else
          lexec(L, oop);
      }
      if (op > opBrackets || (op & 1) == 0)
        ops.push(op);

      if (op == opEnd)
        return lres;
    }
    else if (expr[i].type == lexLit)
    {
      if (val == 0) return eFail;
      if (unres(lres))
        return eFail;
      pushlit(L, expr[i]);
      val = 0;
    }
    else if (expr[i].type == lexName)
    {
      if (val == 0) return eFail;
      val = 0;
      if (unres(lres))
        return eFail;
      if (expr[i].value == "metatable" && expr[i + 1].op == '(')
      {
        i += 2;
        if (doeval() != eValue)
          return eFail;
        if (expr[i].op != ')')
          return eFail;
        if (dores(lres, eMetaTable))
          return eFail;
      }
      else
      {
        if (locals.has(expr[i].value))
        {
          if (dores(lres, locals.get(expr[i].value)))
            return eFail;
        }
        else
        {
          lua_pushglobaltable(L);
          lua_pushstring(L, expr[i].value);
          if (dores(lres, eTableKey))
            return eFail;
        }
      }
    }
  }

  return eFail;
}
int RunContext::doeval()
{
  int top = lua_gettop(L);
  depth++;
  int res = rawdoeval();
  depth--;
  if (res == eTableKey)
    top += 2;
  else if (res == eValue)
    top += 1;
  else if (res == eMetaTable)
    top += 1;
  if (lua_gettop(L) != top)
  {
    lua_settop(L, top);
    return eFail;
  }
  return res;
}

int eval(lua_State* L, int local, char const* str, bool rvalue)
{
  int top = lua_gettop(L);

  RunContext c;
  c.L = L;
  c.lvalue = !rvalue;
  c.i = 0;
  c.depth = 0;
  if (local >= 0 && lua_getstack(L, local, &c.ar))
  {
    for (int i = 1; char const* name = lua_getlocal(L, &c.ar, i); i++)
    {
      c.locals.set(name, i);
      lua_pop(L, 1);
    }
  }

  {
    Lexem lex;
    while (*str)
    {
      str = parse(str, lex);
      if (str == NULL)
        return eFail;
      if (lex.type != lexEnd)
        c.expr.push(lex);
    }
    lex.type = lexEnd;
    lex.op = 0;
    c.expr.push(lex);
  }

  int res = c.doeval();
  if (c.i != c.expr.length() - 1)
  {
    res = eFail;
    if (lua_gettop(L) != top)
      lua_settop(L, top);
  }
  return res;
}
bool islvalue(int code)
{
  return (code == eTableKey || code > 0 || code == eMetaTable);
}
bool islvalue(lua_State* L, int local, char const* str)
{
  int top = lua_gettop(L);
  int res = eval(L, local, str, false);
  lua_settop(L, top);
  return (res == eTableKey || (res > 0 && local >= 0) || res == eMetaTable);
}
int setvalue(lua_State* L, int local, char const* str)
{
  int ret = eval(L, local, str, false);
  if (ret == eTableKey)
  {
    if (!lua_istable(L, -2))
    {
      lua_pop(L, 3);
      return eFail;
    }
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 2);
  }
  else if (ret == eMetaTable)
  {
    if (!lua_istable(L, -1) || (!lua_isnil(L, -2) && !lua_istable(L, -2)))
    {
      lua_pop(L, 2);
      return eFail;
    }
    lua_pushvalue(L, -2);
    lua_setmetatable(L, -2);
    lua_pop(L, 2);
  }
  else if (ret > 0 && local >= 0)
  {
    lua_Debug ar;
    if (!lua_getstack(L, local, &ar) || !lua_setlocal(L, &ar, ret))
    {
      lua_pop(L, 1);
      return eFail;
    }
  }
  else
  {
    lua_pop(L, 1);
    return eFail;
  }
  return ret;
}
int getvalue(lua_State* L, int local, char const* str)
{
  int ret = eval(L, local, str, false);
  if (ret == eTableKey)
  {
    if (!lua_istable(L, -2))
    {
      lua_pop(L, 2);
      return eFail;
    }
    lua_rawget(L, -2);
    lua_remove(L, -2);
  }
  else if (ret == eMetaTable)
  {
    if (!lua_istable(L, -1))
    {
      lua_pop(L, 1);
      return eFail;
    }
    lua_getmetatable(L, -1);
    lua_remove(L, -2);
  }
  else if (ret > 0)
  {
    lua_Debug ar;
    if (!lua_getstack(L, local, &ar) || !lua_getlocal(L, &ar, ret))
      return eFail;
  }
  else if (ret != eValue)
    return eFail;
  return ret;
}

String formatstr(char const* str, int len)
{
  String result;
  if (len < 0)
    len = strlen(str);
  for (int p = 0; p < len; p++)
  {
    switch (str[p])
    {
    case '\a':
      result += "\\a";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    case '\v':
      result += "\\v";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '"':
      result += "\\\"";
      break;
    default:
      if (s_iscntrl(str[p]))
      {
        if (s_isdigit(str[p + 1]))
          result.printf("\\%d", int((unsigned char) str[p]));
        else
          result.printf("\\%03d", int((unsigned char) str[p]));
      }
      else
        result += str[p];
    }
  }
  return result;
}

WideString towstring(lua_State* L, int pos, bool inctype)
{
  int type = lua_type(L, pos);
  switch (type)
  {
  case LUA_TNONE:
    return L"";
  case LUA_TNIL:
    return L"nil";
  case LUA_TBOOLEAN:
    return (lua_toboolean(L, -1) ? L"true" : L"false");
  case LUA_TNUMBER:
    return WideString::format(L"%.14g", lua_tonumber(L, -1));
  case LUA_TSTRING:
    {
      size_t len;
      char const* str = lua_tolstring(L, -1, &len);
      return WideString(String::format("\"%s\"", formatstr(str, len)));
    }
  default:
    if (inctype)
      return WideString(String::format("%s: 0x%p", lua_typename(L, type), lua_topointer(L, -1)));
    else
      return WideString(String::format("0x%p", lua_topointer(L, -1)));
  }
}

}

