#include <lua/lua.hpp>
#include "ilua/ilua.h"
#include "base/string.h"
#include "base/utf8.h"

namespace api
{

static int utf8_valid(lua_State* L)
{
  size_t length;
  uint8 const* str = (uint8*) luaL_checklstring(L, 1, &length);
  size_t pos = 0;
  int valid = 1;
  while (pos < length && valid)
  {
    int tail = 0;
    if (str[pos] <= 0x7F)
      pos++;
    else if (str[pos] >= 0xC2 && str[pos] <= 0xDF)
      pos++, tail = 1;
    else if (str[pos] == 0xE0 && str[pos + 1] >= 0xA0 && str[pos + 1] <= 0xBF)
      pos += 2, tail = 1;
    else if (str[pos] >= 0xE1 && str[pos] <= 0xEC)
      pos++, tail = 2;
    else if (str[pos] == 0xED && str[pos + 1] >= 0x80 && str[pos + 1] <= 0x9F)
      pos += 2, tail = 1;
    else if (str[pos] >= 0xEE && str[pos] <= 0xEF)
      pos++, tail = 2;
    else if (str[pos] == 0xF0 && str[pos + 1] >= 0x90 && str[pos + 1] <= 0xBF)
      pos += 2, tail = 2;
    else if (str[pos] >= 0xF1 && str[pos] <= 0xF3)
      pos++, tail = 3;
    else if (str[pos] == 0xF4 && str[pos + 1] >= 0x80 && str[pos + 1] <= 0x8F)
      pos += 2, tail = 2;
    else
      valid = 0;
    for (int i = 0; i < tail; i++)
    {
      if (str[pos] >= 0x80 && str[pos] <= 0xBF)
        pos++;
      else
      {
        valid = 0;
        break;
      }
    }
  }
  lua_pushboolean(L, valid);
  return 1;
}

static int utf8_byte(lua_State* L)
{
  size_t length;
  uint8 const* str = (uint8*) luaL_checklstring(L, 1, &length);
  int first = luaL_optinteger(L, 2, 1);
  int last = luaL_optinteger(L, 3, first);
  int count = 0;
  int top = lua_gettop(L);
  size_t pos = 0;
  while (pos < length)
  {
    count++;
    if (str[pos] < 0x80)
    {
      if (count >= first && (last < 0 || count <= last))
        lua_pushinteger(L, str[pos]);
      pos++;
    }
    else
    {
      uint32 mask = 0x40;
      uint8 lead = str[pos++];
      uint32 cp = lead;
      while ((lead & 0x40) && pos < length)
      {
        cp = (cp << 6) | (str[pos++] & 0x3F);
        mask <<= 5;
        lead <<= 1;
      }
      if (count >= first && (last < 0 || count <= last))
        lua_pushinteger(L, cp & (mask - 1));
    }
  }
  count = lua_gettop(L) - top;
  if (last < 0)
  {
    count -= (-last - 1);
    if (count < 0)
      count = 0;
    lua_settop(L, top + count);
  }
  return count;
}
static int utf8_char(lua_State* L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  int count = lua_gettop(L);
  for (int i = 1; i <= count; i++)
  {
    uint32 cp = luaL_checkinteger(L, i);
    if (cp <= 0x7F)
      luaL_addchar(&b, cp);
    else if (cp <= 0x7FF)
    {
      luaL_addchar(&b, 0xC0 | (cp >> 6));
      luaL_addchar(&b, 0x80 | (cp & 0x3F));
    }
    else if (cp <= 0xFFFF)
    {
      luaL_addchar(&b, 0xE0 | (cp >> 12));
      luaL_addchar(&b, 0x80 | ((cp >> 6) & 0x3F));
      luaL_addchar(&b, 0x80 | (cp & 0x3F));
    }
    else if (cp <= 0x10FFFF)
    {
      luaL_addchar(&b, 0xF0 | (cp >> 18));
      luaL_addchar(&b, 0x80 | ((cp >> 12) & 0x3F));
      luaL_addchar(&b, 0x80 | ((cp >> 6) & 0x3F));
      luaL_addchar(&b, 0x80 | (cp & 0x3F));
    }
  }
  luaL_pushresult(&b);
  return 1;
}
static int utf8length(uint8 const* str, uint8 const* end)
{
  int count = 0;
  while (str < end)
  {
    str++;
    while (((*str) & 0xC0) == 0x80)
      str++;
    count++;
  }
  return count;
}
static int utf8_len(lua_State* L)
{
  size_t length;
  uint8 const* str = (uint8*) luaL_checklstring(L, 1, &length);
  lua_pushinteger(L, utf8length(str, str + length));
  return 1;
}
static int utf8_reverse(lua_State* L)
{
  size_t length;
  uint8 const* str = (uint8*) luaL_checklstring(L, 1, &length);
  uint8 const* pos = str + length;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while (pos > str)
  {
    uint8 const* end = pos--;
    while (pos > str && ((*pos) & 0xC0) == 0x80)
      pos--;
    luaL_addlstring(&b, (char*) pos, end - pos);
  }
  luaL_pushresult(&b);
  return 1;
}
static int utf8_lower(lua_State* L)
{
  size_t length;
  uint8_const_ptr ptr = (uint8_const_ptr) luaL_checklstring(L, 1, &length);
  uint8_const_ptr end = (ptr + length);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while (ptr < end)
  {
    uint32 cp = utf8::transform(&ptr, utf8::tf_lower);
    if (cp == 0)
      luaL_addchar(&b, 0);
    else
    {
      while (cp)
      {
        luaL_addchar(&b, char(cp));
        cp >>= 8;
      }
    }
  }
  luaL_pushresult(&b);
  return 1;
}
static int utf8_upper(lua_State* L)
{
  size_t length;
  uint8_const_ptr ptr = (uint8_const_ptr) luaL_checklstring(L, 1, &length);
  uint8_const_ptr end = (ptr + length);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while (ptr < end)
  {
    uint32 cp = utf8::transform(&ptr, utf8::tf_upper);
    if (cp == 0)
      luaL_addchar(&b, 0);
    else
    {
      while (cp)
      {
        luaL_addchar(&b, char(cp));
        cp >>= 8;
      }
    }
  }
  luaL_pushresult(&b);
  return 1;
}
static int utf8offset(size_t length, uint8 const* str, int index)
{
  if (index > 0)
  {
    size_t pos = 0;
    while (pos < length && --index)
    {
      pos++;
      while ((str[pos] & 0xC0) == 0x80)
        pos++;
    }
    return pos;
  }
  else
  {
    int pos = length;
    while (pos > 0 && index++)
    {
      pos--;
      while (pos > 0 && (str[pos] & 0xC0) == 0x80)
        pos--;
    }
    return pos;
  }
}
static int utf8_offset(lua_State* L)
{
  size_t length;
  uint8 const* str = (uint8*) luaL_checklstring(L, 1, &length);
  int index = luaL_checkinteger(L, 2);
  lua_pushinteger(L, utf8offset(length, str, index) + 1);
  return 1;
}
static int utf8_fromoffset(lua_State* L)
{
  size_t length;
  uint8 const* str = (uint8*) luaL_checklstring(L, 1, &length);
  int offset = luaL_checkinteger(L, 2);
  if (offset < 0)
    offset += length + 1;
  if (offset > length)
    offset = length;
  lua_pushinteger(L, utf8length(str, str + offset));
  return 1;
}
static int utf8_sub(lua_State* L)
{
  size_t length;
  uint8 const* str = (uint8*) luaL_checklstring(L, 1, &length);
  int i = luaL_checkinteger(L, 2);
  int j = luaL_optinteger(L, 3, -1);
  i = utf8offset(length, str, i);
  j = utf8offset(length, str, j + 1);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  if (i < j)
    luaL_addlstring(&b, (char*) str + i, j - i);
  luaL_pushresult(&b);
  return 1;
}

static size_t fixpos(int pos, size_t len)
{
  if (pos >= 0)
    return pos;
  else if (0u - (size_t) pos > len)
    return 0;
  else
    return len - ((size_t) -pos) + 1;
}
static int utf8_unpack(lua_State* L)
{
  size_t length;
  uint8_const_ptr ptr = (uint8_const_ptr) luaL_checklstring(L, 1, &length);
  size_t p0 = fixpos(luaL_optinteger(L, 2, 1), length);
  size_t p1 = fixpos(luaL_optinteger(L, 3, length), length);
  if (p0 < 1) p0 = 1;
  if (p1 > length) p1 = length;
  if (p0 > p1) return 0;
  uint8_const_ptr end = (ptr + p1);
  ptr += p0 - 1;
  int top = lua_gettop(L);
  while (ptr < end)
  {
    uint8_const_ptr prev = ptr;
    utf8::transform(&ptr, NULL);
    lua_pushlstring(L, (char const*) prev, ptr - prev);
  }
  return lua_gettop(L) - top;
}
static int string_unpack(lua_State* L)
{
  size_t length;
  char const* ptr = luaL_checklstring(L, 1, &length);
  size_t p0 = fixpos(luaL_optinteger(L, 2, 1), length);
  size_t p1 = fixpos(luaL_optinteger(L, 3, length), length);
  if (p0 < 1) p0 = 1;
  if (p1 > length) p1 = length;
  if (p0 > p1) return 0;
  int top = lua_gettop(L);
  for (size_t i = p0 - 1; i < p1; i++)
    lua_pushlstring(L, ptr + i, 1);
  return lua_gettop(L) - top;
}
static int string_concat(lua_State* L)
{
  int n = lua_gettop(L);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for (int i = 1; i <= n; i++)
  {
    size_t length;
    char const* str = luaL_checklstring(L, i, &length);
    luaL_addlstring(&b, str, length);
  }
  luaL_pushresult(&b);
  return 1;
}

void bind_utf8(lua_State* L)
{
  ilua::openlib(L, "utf8");
  ilua::bindmethod(L, "valid", utf8_valid);
  ilua::bindmethod(L, "byte", utf8_byte);
  ilua::bindmethod(L, "char", utf8_char);
  ilua::bindmethod(L, "len", utf8_len);
  ilua::bindmethod(L, "lower", utf8_lower);
  ilua::bindmethod(L, "upper", utf8_upper);
  ilua::bindmethod(L, "reverse", utf8_reverse);
  ilua::bindmethod(L, "offset", utf8_offset);
  ilua::bindmethod(L, "fromoffset", utf8_fromoffset);
  ilua::bindmethod(L, "sub", utf8_sub);
  ilua::bindmethod(L, "unpack", utf8_unpack);
  lua_pop(L, 1);

  ilua::openlib(L, "string");
  ilua::bindmethod(L, "unpack", string_unpack);
  ilua::bindmethod(L, "concat", string_concat);
  lua_pop(L, 1);
}

}
