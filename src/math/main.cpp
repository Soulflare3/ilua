#include <windows.h>
#include <lua/lua.hpp>
#include "bignum.h"
#include "hash.h"
#include "strconv.h"
#include "rand.h"

void bignum_bind(lua_State* L);

extern "C" __declspec(dllexport) void StartModule(lua_State* L)
{
  bignum_bind(L);
  hash_bind(L);
  strconv_bind(L);
  random_bind(L);
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
  return TRUE;
}
