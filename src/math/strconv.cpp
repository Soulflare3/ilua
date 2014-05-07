#include "strconv.h"
#include "base/types.h"
#include "ilua/ilua.h"
#include <ctype.h>

#define NA        255
static const char base64_e[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const uint8 base64_d[96] = {
  NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,62,NA,NA,NA,63,
  52,53,54,55,56,57,58,59,60,61,NA,NA,NA,NA,NA,NA,
  NA, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
  15,16,17,18,19,20,21,22,23,24,25,NA,NA,NA,NA,NA,
  NA,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
  41,42,43,44,45,46,47,48,49,50,51,NA,NA,NA,NA,NA,
};
static void base64_doencode(luaL_Buffer* b, uint8 const* c, int n)
{
  uint32 val = 0;
  for (int i = 0; i < n; i++) val = (val << 8) | (*c++);
  for (int i = n; i < 3; i++) val <<= 8;
  char res[4];
  res[3] = base64_e[val & 0x3F], val >>= 6;
  res[2] = base64_e[val & 0x3F], val >>= 6;
  res[1] = base64_e[val & 0x3F], val >>= 6;
  res[0] = base64_e[val & 0x3F];
  if (n < 2) res[2] = '=';
  if (n < 3) res[3] = '=';
  luaL_addlstring(b, res, 4);
}
static int base64_encode(lua_State* L)
{
  size_t length;
  uint8 const* ptr = (uint8*) luaL_checklstring(L, 1, &length);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while (length >= 3)
  {
    base64_doencode(&b, ptr, 3);
    ptr += 3;
    length -= 3;
  }
  if (length)
    base64_doencode(&b, ptr, length);
  luaL_pushresult(&b);
  return 1;
}
static void base64_dodecode(luaL_Buffer* b, uint32* c, int n)
{
  uint32 val = (c[0] << 18) | (c[1] << 12) | (c[2] << 6) | c[3];
  char res[3];
  res[0] = (val >> 16);
  res[1] = (val >> 8);
  res[2] = val;
  luaL_addlstring(b, res, n - 1);
}
static int base64_decode(lua_State* L)
{
  char const* ptr = luaL_checkstring(L, 1);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  int n = 0;
  uint32 t[4] = {0, 0, 0, 0};
  while (*ptr)
  {
    char c = *ptr++;
    if (c == '=')
      base64_dodecode(&b, t, n);
    else if (c >= 0x20 && c < 0x80 && base64_d[c] != NA)
    {
      t[n++] = base64_d[c];
      if (n == 4)
        base64_dodecode(&b, t, n), n = 0;
    }
    else if (!isspace((unsigned char) c))
      return 0;
  }
  luaL_pushresult(&b);
  return 1;
}

static void base85_doencode(luaL_Buffer* b, uint8 const* c, int n)
{
  uint32 val = 0;
  for (int i = 0; i < n; i++) val = (val << 8) | (*c++);
  for (int i = n; i < 4; i++) val <<= 8;
  if (c == 0 && n == 4)
    luaL_addchar(b, 'z');
  else
  {
    char res[5];
    res[4] = '!' + (val % 85); val /= 85;
    res[3] = '!' + (val % 85); val /= 85;
    res[2] = '!' + (val % 85); val /= 85;
    res[1] = '!' + (val % 85); val /= 85;
    res[0] = '!' + (val % 85);
    luaL_addlstring(b, res, n + 1);
  }
}
static int base85_encode(lua_State* L)
{
  size_t length;
  uint8 const* ptr = (uint8*) luaL_checklstring(L, 1, &length);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addstring(&b, "<~");
  while (length >= 4)
  {
    base85_doencode(&b, ptr, 4);
    ptr += 4;
    length -= 4;
  }
  if (length)
    base85_doencode(&b, ptr, length);
  luaL_addstring(&b, "~>");
  luaL_pushresult(&b);
  return 1;
}
static void base85_dodecode(luaL_Buffer* b, uint32* c, int n)
{
  for (int i = n; i < 5; i++) c[i] = 84;
  uint32 val = (((c[0] * 85 + c[1]) * 85 + c[2]) * 85 + c[3]) * 85 + c[4];
  char res[4];
  res[0] = (val >> 24);
  res[1] = (val >> 16);
  res[2] = (val >> 8);
  res[3] = val;
  luaL_addlstring(b, res, n - 1);
}
static int base85_decode(lua_State* L)
{
  char const* ptr = luaL_checkstring(L, 1);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  if (ptr[0] == '<' && ptr[1] == '~') ptr += 2;
  int n = 0;
  uint32 t[5] = {0, 0, 0, 0, 0};
  while (*ptr)
  {
    char c = *ptr++;
    if (c == 'z')
      luaL_addlstring(&b, "\0\0\0\0", 4);
    else if (c == '~')
    {
      if (*ptr != '>') return 0;
      base85_dodecode(&b, t, n);
      break;
    }
    else if (c >= '!' && c <= 'u')
    {
      t[n++] = c - '!';
      if (n == 5)
        base64_dodecode(&b, t, n), n = 0;
    }
    else if (!isspace((unsigned char) c))
      return 0;
  }
  luaL_pushresult(&b);
  return 1;
}

void strconv_bind(lua_State* L)
{
  ilua::openlib(L, "base64");
  ilua::bindmethod(L, "encode", base64_encode);
  ilua::bindmethod(L, "decode", base64_decode);
  lua_pop(L, 1);

  ilua::openlib(L, "ascii85");
  ilua::bindmethod(L, "encode", base85_encode);
  ilua::bindmethod(L, "decode", base85_decode);
  lua_pop(L, 1);
}
