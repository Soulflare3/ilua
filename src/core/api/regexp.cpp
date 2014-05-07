#include <lua/lua.hpp>
#include "ilua/ilua.h"
#include "base/string.h"
#define PCRE_STATIC
#include "pcre/pcre.h"
#include "base/utf8.h"

namespace api
{

#define RE_CACHE        "ilua_re_cache"
#define RE_LIST         0x80000000

struct ReProg
{
  pcre* prog;
  int captures;
  int flags;
  ReProg(lua_State* L, pcre* p, char const* text, int f)
    : prog(p)
    , captures(0)
    , flags(f)
  {
    pcre_fullinfo(prog, NULL, PCRE_INFO_CAPTURECOUNT, &captures);
    pcre_fullinfo(prog, NULL, PCRE_INFO_OPTIONS, &flags);
    flags |= (f & RE_LIST);

    if (ilua::totable(L, -1))
    {
      ilua::settabss(L, "pattern", text);
      ilua::settabsi(L, "flags", flags);
      ilua::settabsi(L, "groups", captures);
      lua_newtable(L);
      int count, entrysize;
      uint8* table;
      if (pcre_fullinfo(prog, NULL, PCRE_INFO_NAMECOUNT, &count) &&
          pcre_fullinfo(prog, NULL, PCRE_INFO_NAMEENTRYSIZE, &entrysize) &&
          pcre_fullinfo(prog, NULL, PCRE_INFO_NAMETABLE, &table))
      {
        for (int i = 0; i < count; i++)
        {
          ilua::settabsi(L, (char*) (table + 2), (int(table[0]) << 8) | table[1]);
          table += entrysize;
        }
      }
      lua_setfield(L, -2, "groupindex");
      lua_pop(L, 1);
    }
  }
  ~ReProg()
  {
    pcre_free(prog);
  }
};
struct ReMatch
{
  ReProg* prog;
  size_t length;
  char* text;
  int* ovector;
  int pos;
  ReMatch(lua_State* L, ReProg* p, size_t l, char const* t, int* o, int ps)
    : prog(p)
    , length(l)
    , ovector(o)
    , pos(ps)
  {
    text = new char[length + 1];
    memcpy(text, t, length);
    text[length] = 0;

    if (ilua::totable(L, -1))
    {
      ilua::settabsi(L, "pos", pos + 1);
      lua_pushvalue(L, 1);
      lua_setfield(L, -2, "re");
      lua_pushlstring(L, text, length);
      lua_setfield(L, -2, "string");
      lua_pop(L, 1);
    }
  }
  ~ReMatch()
  {
    delete[] text;
    delete[] ovector;
  }
};

static ReProg* getprog(lua_State* L, int pos, int flags = 0)
{
  pos = lua_absindex(L, pos);
  if (lua_type(L, pos) == LUA_TSTRING)
  {
    lua_getfield(L, LUA_REGISTRYINDEX, RE_CACHE);
    lua_pushvalue(L, pos);
    lua_rawget(L, -2);
    ReProg* rp = (ReProg*) lua_touserdata(L, -1);
    if (rp && rp->flags == flags)
    {
      lua_pop(L, 2);
      return rp;
    }
    lua_pop(L, 1);
    lua_pushvalue(L, pos);

    char const* pattern = lua_tostring(L, pos);
    char const* errptr;
    int erroffset;
    pcre* prog = pcre_compile(pattern, (flags & (~RE_LIST)) | PCRE_UTF8, &errptr, &erroffset, NULL);
    if (prog == NULL)
      luaL_error(L, "error compiling regex at position %d: %s", erroffset, errptr);
    rp = new(L, "re.prog") ReProg(L, prog, pattern, flags);
    lua_pushvalue(L, -1);
    lua_replace(L, pos);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    return rp;
  }
  else
  {
    ReProg* prog = ilua::toobject<ReProg>(L, pos, "re.prog");
    if (prog == NULL) luaL_argerror(L, pos, "string or expression expected");
    return prog;
  }
}

static int re_compile(lua_State* L)
{
  char const* pattern = luaL_checkstring(L, 1);
  int flags = luaL_optinteger(L, 2, 0);
  char const* errptr;
  int erroffset;
  pcre* prog = pcre_compile(pattern, (flags & (~RE_LIST)) | PCRE_UTF8, &errptr, &erroffset, NULL);
  if (prog == NULL)
  {
    lua_pushnil(L);
    lua_pushstring(L, errptr);
    lua_pushinteger(L, erroffset);
    return 3;
  }
  ReProg* rp = new(L, "re.prog") ReProg(L, prog, pattern, flags);
  return 1;
}

static int re_dosearch(lua_State* L, ReProg* prog, int pos, int flags)
{
  size_t length;
  char const* text = luaL_checklstring(L, 2, &length);
  if (pos < 0)
    pos += length + 1;
  int ovecsize = (prog->captures + 1) * 3;
  int* ovector = new int[ovecsize];
  int rc = pcre_exec(prog->prog, NULL, text, length, pos, flags, ovector, ovecsize);
  if (rc < 0)
  {
    delete[] ovector;
    lua_pushnil(L);
    return 1;
  }
  if (prog->flags & RE_LIST)
  {
    for (int i = 0; i <= prog->captures; i++)
    {
      if (i < rc && ovector[i + i] >= 0)
        lua_pushlstring(L, text + ovector[i + i], ovector[i + i + 1] - ovector[i + i]);
      else
        lua_pushnil(L);
    }
    delete[] ovector;
    return prog->captures + 1;
  }
  for (int i = rc; i <= prog->captures; i++)
    ovector[i + i] = ovector[i + i + 1] = -1;
  new(L, "re.matchinfo") ReMatch(L, prog, length, text, ovector, pos);
  return 1;
}
static int re_search(lua_State* L)
{
  ReProg* prog = getprog(L, 1, luaL_optinteger(L, 3, RE_LIST));
  return re_dosearch(L, prog, 0, 0);
}
static int re_match(lua_State* L)
{
  ReProg* prog = getprog(L, 1, luaL_optinteger(L, 3, RE_LIST));
  return re_dosearch(L, prog, 0, PCRE_ANCHORED);
}
static int rep_search(lua_State* L)
{
  ReProg* prog = ilua::checkobject<ReProg>(L, 1, "re.prog");
  return re_dosearch(L, prog, luaL_optinteger(L, 3, 1) - 1, 0);
}
static int rep_match(lua_State* L)
{
  ReProg* prog = ilua::checkobject<ReProg>(L, 1, "re.prog");
  return re_dosearch(L, prog, luaL_optinteger(L, 3, 1) - 1, PCRE_ANCHORED);
}

static int re_split(lua_State* L)
{
  ReProg* prog = getprog(L, 1, luaL_optinteger(L, 4, 0));
  int maxsplit = luaL_optinteger(L, 3, 0);
  size_t length;
  char const* text = luaL_checklstring(L, 2, &length);

  int ovecsize = (prog->captures + 1) * 3;
  int* ovector = new int[ovecsize];

  int top = lua_gettop(L);

  int prev = 0, rc, count = 0;
  while ((maxsplit == 0 || count < maxsplit) &&
        (rc = pcre_exec(prog->prog, NULL, text, length, prev, PCRE_NOTEMPTY, ovector, ovecsize)) > 0 && ovector[1] >= 0)
  {
    lua_pushlstring(L, text + prev, ovector[0] - prev);
    for (int i = 1; i <= prog->captures && i < rc; i++)
      if (ovector[i + i] >= 0)
        lua_pushlstring(L, text + ovector[i + i], ovector[i + i + 1] - ovector[i + i]);
    prev = ovector[1];
    count++;
  }
  lua_pushlstring(L, text + prev, length - prev);

  delete[] ovector;

  return lua_gettop(L) - top;
}

static int re_dofindall(lua_State* L, ReProg* prog, int pos, int flags)
{
  size_t length;
  char const* text = luaL_checklstring(L, 2, &length);
  if (pos < 0)
    pos += length + 1;
  int ovecsize = (prog->captures + 1) * 3;
  int* ovector = new int[ovecsize];
  int rc;
  int top = lua_gettop(L);
  while (pos <= length && (rc = pcre_exec(prog->prog, NULL, text, length, pos, 0, ovector, ovecsize)) > 0 && ovector[1] >= 0)
  {
    if (prog->captures <= 1 && prog->captures < rc && ovector[prog->captures * 2] >= 0)
      lua_pushlstring(L, text + ovector[prog->captures * 2], ovector[prog->captures * 2 + 1] - ovector[prog->captures * 2]);
    else if (prog->captures <= 1)
      lua_pushnil(L);
    else
    {
      lua_newtable(L);
      for (int i = 1; i <= prog->captures; i++)
      {
        if (i < rc && ovector[i + i] >= 0)
        {
          lua_pushlstring(L, text + ovector[i + i], ovector[i + i + 1] - ovector[i + i]);
          lua_rawseti(L, -2, i);
        }
      }
    }
    pos = ovector[1] + (ovector[0] == ovector[1]);
  }
  delete[] ovector;
  return lua_gettop(L) - top;
}
static int re_findall(lua_State* L)
{
  ReProg* prog = getprog(L, 1, luaL_optinteger(L, 3, RE_LIST));
  return re_dofindall(L, prog, 0, 0);
}
static int rep_findall(lua_State* L)
{
  ReProg* prog = ilua::checkobject<ReProg>(L, 1, "re.prog");
  return re_dofindall(L, prog, luaL_optinteger(L, 3, 1) - 1, 0);
}

static int re_finditercb(lua_State* L)
{
  ReMatch* rm = ilua::checkobject<ReMatch>(L, 1, "re.matchinfo");
  int pos = (rm->ovector[1] >= 0 ? rm->ovector[1] + (rm->ovector[0] == rm->ovector[1]) : rm->pos);
  int rc = pcre_exec(rm->prog->prog, NULL, rm->text, rm->length, pos, 0, rm->ovector, (rm->prog->captures + 1) * 3);
  if (rc < 0 || rm->ovector[1] < 0)
  {
    lua_pushnil(L);
    return 1;
  }
  for (int i = rc; i <= rm->prog->captures; i++)
    rm->ovector[i + i] = rm->ovector[i + i + 1] = -1;
  if (rm->prog->flags & RE_LIST)
  {
    for (int i = 0; i <= rm->prog->captures; i++)
    {
      if (i < rc && rm->ovector[i + i] >= 0)
        lua_pushlstring(L, rm->text + rm->ovector[i + i], rm->ovector[i + i + 1] - rm->ovector[i + i]);
      else
        lua_pushnil(L);
    }
    return rm->prog->captures + 1;
  }
  lua_settop(L, 1);
  return 1;
}

static int re_dofinditer(lua_State* L, ReProg* prog, int pos, int flags)
{
  size_t length;
  char const* text = luaL_checklstring(L, 2, &length);
  if (pos < 0)
    pos += length + 1;
  int ovecsize = (prog->captures + 1) * 3;
  int* ovector = new int[ovecsize];
  for (int i = 0; i < ovecsize; i++)
    ovector[i] = -1;
  lua_pushcfunction(L, re_finditercb);
  new(L, "re.matchinfo") ReMatch(L, prog, length, text, ovector, pos);
  lua_pushnil(L);
  return 3;
}
static int re_finditer(lua_State* L)
{
  ReProg* prog = getprog(L, 1, luaL_optinteger(L, 3, RE_LIST));
  return re_dofinditer(L, prog, 0, 0);
}
static int rep_finditer(lua_State* L)
{
  ReProg* prog = ilua::checkobject<ReProg>(L, 1, "re.prog");
  return re_dofinditer(L, prog, luaL_optinteger(L, 3, 1) - 1, 0);
}

static void re_doexpand(ReMatch* rm, luaL_Buffer* b, size_t length, char const* repl, lua_State* L = NULL, int n1 = 0, int n2 = 0)
{
  for (size_t i = 0; i < length; i++)
  {
    if (repl[i] == '\\')
    {
      switch (repl[++i])
      {
      case 'a':
        luaL_addchar(b, '\a');
        break;
      case 'b':
        luaL_addchar(b, '\b');
        break;
      case 'f':
        luaL_addchar(b, '\f');
        break;
      case 'n':
        luaL_addchar(b, '\n');
        break;
      case 'r':
        luaL_addchar(b, '\r');
        break;
      case 't':
        luaL_addchar(b, '\t');
        break;
      case 'v':
        luaL_addchar(b, '\v');
        break;
      case 'g':
        if (repl[i + 1] == '<')
        {
          i++;
          String name;
          while (i < length && repl[i] != '>')
            name += repl[i++];
          int id = -1;
          if (name.length() > 0 && name.isDigits())
            id = name.toInt();
          else if (rm)
            id = pcre_get_stringnumber(rm->prog->prog, name);
          if (rm)
          {
            if (id >= 0 && id <= rm->prog->captures && rm->ovector[id + id] >= 0)
              luaL_addlstring(b, rm->text + rm->ovector[id + id], rm->ovector[id + id + 1] - rm->ovector[id + id]);
          }
          else if (L && id >= 0 && id <= n2 - n1)
          {
            lua_pushvalue(L, n1 + id);
            luaL_addvalue(b);
          }
        }
      default:
        if (s_isdigit(repl[i]))
        {
          int id = 0;
          for (int j = 0; j < 2 && s_isdigit(repl[i]); j++)
            id = id * 10 + int(repl[i++] - '0');
          if (rm)
          {
            if (id >= 0 && id <= rm->prog->captures && rm->ovector[id + id] >= 0)
              luaL_addlstring(b, rm->text + rm->ovector[id + id], rm->ovector[id + id + 1] - rm->ovector[id + id]);
          }
          else if (L && id >= 0 && id <= n2 - n1)
          {
            lua_pushvalue(L, n1 + id);
            luaL_addvalue(b);
          }
          i--;
        }
        else
          luaL_addchar(b, repl[i]);
      }
    }
    else
      luaL_addchar(b, repl[i]);
  }
}

struct ReSub
{
  int match;
  int maxcount;
  int count;
  luaL_Buffer b;
};
static int re_sub_continue(lua_State* L)
{
  int result = 0;
  int ctx = lua_gettop(L);
  if (lua_getctx(L, &ctx) == LUA_YIELD)
    result = 1;

  ReSub* rs = (ReSub*) lua_touserdata(L, ctx);
  ReMatch* rm = (ReMatch*) lua_touserdata(L, rs->match);
  luaL_Buffer* b = &rs->b;

  while (true)
  {
    if (result == 0)
    {
      int pos = rm->ovector[1] + (rm->ovector[0] == rm->ovector[1]);
      int prev = rm->ovector[1] + (rm->ovector[1] == -1);
      int rc;
      if (rs->maxcount == 0 || rs->count < rs->maxcount)
        rc = pcre_exec(rm->prog->prog, NULL, rm->text, rm->length, pos, 0, rm->ovector, (rm->prog->captures + 1) * 3);
      else
        rc = -1;
      if (rc < 0 || rm->ovector[0] < 0)
      {
        luaL_addlstring(b, rm->text + prev, rm->length - prev);
        break;
      }
      for (int i = rc; i <= rm->prog->captures; i++)
        rm->ovector[i + i] = rm->ovector[i + i + 1] = -1;
      luaL_addlstring(b, rm->text + prev, rm->ovector[0] - prev);
      rs->count++;
    }
    if (lua_isfunction(L, 2))
    {
      if (result == 0)
      {
        lua_pushvalue(L, 2);
        if (rm->prog->flags & RE_LIST)
        {
          for (int i = 0; i <= rm->prog->captures; i++)
          {
            if (rm->ovector[i + i] >= 0)
              lua_pushlstring(L, rm->text + rm->ovector[i + i], rm->ovector[i + i + 1] - rm->ovector[i + i]);
            else
              lua_pushnil(L);
          }
          lua_callk(L, rm->prog->captures + 1, 1, ctx, re_sub_continue);
        }
        else
        {
          lua_pushvalue(L, rs->match);
          lua_callk(L, 1, 1, ctx, re_sub_continue);
        }
      }
      result = 0;
      luaL_addvalue(b);
    }
    else
    {
      size_t length;
      char const* repl = lua_tolstring(L, 2, &length);
      re_doexpand(rm, b, length, repl);
    }
  }
  luaL_pushresult(b);
  lua_pushinteger(L, rs->count);
  return 2;
}

static int re_sub(lua_State* L)
{
  ReProg* prog = getprog(L, 1, luaL_optinteger(L, 5, RE_LIST));
  int maxcount = luaL_optinteger(L, 4, 0);
  size_t length;
  char const* text = luaL_checklstring(L, 3, &length);
  int ovecsize = (prog->captures + 1) * 3;
  int* ovector = new int[ovecsize];
  for (int i = 0; i < ovecsize; i++)
    ovector[i] = -1;
  new(L, "re.matchinfo") ReMatch(L, prog, length, text, ovector, 0);
  ReSub* rs = (ReSub*) lua_newuserdata(L, sizeof(ReSub));
  rs->match = lua_gettop(L) - 1;
  rs->maxcount = maxcount;
  rs->count = 0;
  luaL_buffinit(L, &rs->b);
  return re_sub_continue(L);
}

static int re_escape(lua_State* L)
{
  size_t length;
  uint8_const_ptr ptr = (uint8_const_ptr) luaL_checklstring(L, 1, &length);
  uint8_const_ptr end = ptr + length;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while (ptr < end)
  {
    uint8_const_ptr prev = ptr;
    uint32 cp = utf8::parse(utf8::transform(&ptr, NULL));
    switch (cp)
    {
    case 0:
      luaL_addlstring(&b, "\\0", 2);
      break;
    case '\a':
      luaL_addlstring(&b, "\\a", 2);
      break;
    case 0x1B:
      luaL_addlstring(&b, "\\e", 2);
      break;
    case '\f':
      luaL_addlstring(&b, "\\f", 2);
      break;
    case '\n':
      luaL_addlstring(&b, "\\n", 2);
      break;
    case '\r':
      luaL_addlstring(&b, "\\r", 2);
      break;
    case '\t':
      luaL_addlstring(&b, "\\t", 2);
      break;
    default:
      if (iswprint(cp))
      {
        if (!iswalnum(cp))
          luaL_addchar(&b, '\\');
        luaL_addlstring(&b, (char const*) prev, ptr - prev);
      }
      else if (cp < 256)
        luaL_printf(&b, "\\x%02X", cp);
      else
        luaL_printf(&b, "\\x{%X}", cp);
    }
  }
  luaL_pushresult(&b);
  return 1;
}
static int re_expand(lua_State* L)
{
  size_t length;
  char const* repl = luaL_checklstring(L, 1, &length);
  ReMatch* match = ilua::toobject<ReMatch>(L, 2, "re.matchinfo");
  int n = lua_gettop(L);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  if (match)
    re_doexpand(match, &b, length, repl);
  else
  {
    for (int i = 2; i <= n; i++)
      luaL_checkstring(L, i);
    re_doexpand(NULL, &b, length, repl, L, 2, n);
  }
  luaL_pushresult(&b);
  return 1;
}

static int rem_expand(lua_State* L)
{
  ReMatch* match = ilua::checkobject<ReMatch>(L, 1, "re.matchinfo");
  size_t length;
  char const* repl = luaL_checklstring(L, 2, &length);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  re_doexpand(match, &b, length, repl);
  luaL_pushresult(&b);
  return 1;
}
static int checkgroup(lua_State* L, int pos, ReProg* prog)
{
  if (lua_type(L, pos) == LUA_TNUMBER)
  {
    int idx = lua_tointeger(L, pos);
    if (idx < 0 || idx > prog->captures)
      luaL_argerror(L, pos, "group index out of bounds");
    return idx;
  }
  if (lua_type(L, pos) == LUA_TSTRING)
  {
    char const* str = lua_tostring(L, pos);
    int idx = pcre_get_stringnumber(prog->prog, str);
    if (idx < 0 || idx > prog->captures)
      luaL_argerror(L, pos, "unknown group name");
    return idx;
  }
  return luaL_argerror(L, pos, "invalid capture group index");
}
static int rem_group(lua_State* L)
{
  ReMatch* match = ilua::checkobject<ReMatch>(L, 1, "re.matchinfo");
  if (lua_gettop(L) == 1)
    lua_pushinteger(L, 0);
  int n = lua_gettop(L) - 1;
  for (int k = 2; k <= n + 1; k++)
  {
    int i = checkgroup(L, k, match->prog);
    if (i >= 0 && i <= match->prog->captures && match->ovector[i + i] >= 0)
      lua_pushlstring(L, match->text + match->ovector[i + i], match->ovector[i + i + 1] - match->ovector[i + i]);
    else
      lua_pushnil(L);
  }
  return n;
}
static int rem_groups(lua_State* L)
{
  ReMatch* match = ilua::checkobject<ReMatch>(L, 1, "re.matchinfo");
  if (lua_gettop(L) == 1)
    lua_pushnil(L);
  else
    luaL_checkstring(L, 2);
  for (int i = 1; i <= match->prog->captures; i++)
  {
    if (match->ovector[i + i] >= 0)
      lua_pushlstring(L, match->text + match->ovector[i + i], match->ovector[i + i + 1] - match->ovector[i + i]);
    else
      lua_pushvalue(L, 2);
  }
  return match->prog->captures;
}
static int rem_groupdict(lua_State* L)
{
  ReMatch* match = ilua::checkobject<ReMatch>(L, 1, "re.matchinfo");
  if (lua_gettop(L) == 1)
    lua_pushnil(L);
  else
    luaL_checkstring(L, 2);
  lua_newtable(L);
  int count, entrysize;
  uint8* table;
  if (pcre_fullinfo(match->prog->prog, NULL, PCRE_INFO_NAMECOUNT, &count) ||
      pcre_fullinfo(match->prog->prog, NULL, PCRE_INFO_NAMEENTRYSIZE, &entrysize) ||
      pcre_fullinfo(match->prog->prog, NULL, PCRE_INFO_NAMETABLE, &table))
    return 1;
  for (int k = 0; k < count; k++)
  {
    int i = (int(table[0]) << 8) | table[1];
    if (match->ovector[i + i] >= 0)
      lua_pushlstring(L, match->text + match->ovector[i + i], match->ovector[i + i + 1] - match->ovector[i + i]);
    else
      lua_pushvalue(L, 2);
    lua_setfield(L, -2, (char*) (table + 2));
    table += entrysize;
  }
  return 1;
}
static int rem_left(lua_State* L)
{
  ReMatch* match = ilua::checkobject<ReMatch>(L, 1, "re.matchinfo");
  int group = (lua_isnoneornil(L, 2) ? 0 : checkgroup(L, 2, match->prog));
  if (group < 0 || group > match->prog->captures || match->ovector[group + group] < 0)
    lua_pushnil(L);
  else
    lua_pushinteger(L, match->ovector[group + group] + 1);
  return 1;
}
static int rem_right(lua_State* L)
{
  ReMatch* match = ilua::checkobject<ReMatch>(L, 1, "re.matchinfo");
  int group = (lua_isnoneornil(L, 2) ? 0 : checkgroup(L, 2, match->prog));
  if (group < 0 || group > match->prog->captures || match->ovector[group + group + 1] < 0)
    lua_pushnil(L);
  else
    lua_pushinteger(L, match->ovector[group + group + 1]);
  return 1;
}
static int rem_span(lua_State* L)
{
  ReMatch* match = ilua::checkobject<ReMatch>(L, 1, "re.matchinfo");
  int group = (lua_isnoneornil(L, 2) ? 0 : checkgroup(L, 2, match->prog));
  if (group < 0 || group > match->prog->captures || match->ovector[group + group] < 0)
  {
    lua_pushnil(L);
    lua_pushnil(L);
  }
  else
  {
    lua_pushinteger(L, match->ovector[group + group] + 1);
    lua_pushinteger(L, match->ovector[group + group + 1]);
  }
  return 2;
}

void bind_re(lua_State* L)
{
  ilua::newtype<ReProg>(L, "re.prog");
  ilua::bindmethod(L, "search", rep_search);
  ilua::bindmethod(L, "match", rep_match);
  ilua::bindmethod(L, "split", re_split);
  ilua::bindmethod(L, "findall", rep_findall);
  ilua::bindmethod(L, "finditer", rep_finditer);
  ilua::bindmethod(L, "sub", re_sub);
  lua_pop(L, 2);

  ilua::newtype<ReMatch>(L, "re.matchinfo");
  ilua::bindmethod(L, "expand", rem_expand);
  ilua::bindmethod(L, "group", rem_group);
  ilua::bindmethod(L, "groups", rem_groups);
  ilua::bindmethod(L, "groupdict", rem_groupdict);
  ilua::bindmethod(L, "left", rem_left);
  ilua::bindmethod(L, "right", rem_right);
  ilua::bindmethod(L, "span", rem_span);
  lua_pop(L, 2);

  ilua::openlib(L, "re");
  ilua::bindmethod(L, "compile", re_compile);
  ilua::bindmethod(L, "search", re_search);
  ilua::bindmethod(L, "match", re_match);
  ilua::bindmethod(L, "split", re_split);
  ilua::bindmethod(L, "findall", re_findall);
  ilua::bindmethod(L, "finditer", re_finditer);
  ilua::bindmethod(L, "sub", re_sub);
  ilua::bindmethod(L, "escape", re_escape);
  ilua::bindmethod(L, "expand", re_expand);

  ilua::settabsi(L, "I", PCRE_CASELESS);
  ilua::settabsi(L, "IGNORECASE", PCRE_CASELESS);

  ilua::settabsi(L, "L", RE_LIST);
  ilua::settabsi(L, "LIST", RE_LIST);

  ilua::settabsi(L, "M", PCRE_MULTILINE);
  ilua::settabsi(L, "MULTILINE", PCRE_MULTILINE);

  ilua::settabsi(L, "S", PCRE_DOTALL);
  ilua::settabsi(L, "DOTALL", PCRE_DOTALL);

  ilua::settabsi(L, "X", PCRE_EXTENDED);
  ilua::settabsi(L, "VERBOSE", PCRE_EXTENDED);
  lua_pop(L, 1);

  lua_newtable(L);
  lua_newtable(L);
  ilua::settabss(L, "__mode", "kv");
  lua_setmetatable(L, -2);
  lua_setfield(L, LUA_REGISTRYINDEX, RE_CACHE);
}

}
