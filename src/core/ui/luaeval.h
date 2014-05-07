#ifndef __LUA_EVAL__
#define __LUA_EVAL__

#include <lua/lua.hpp>
#include "base/wstring.h"

namespace luae
{

enum {eFail = -3, eMetaTable = -2, eTableKey = -1, eValue = 0};

bool islvalue(lua_State* L, int local, char const* str);
int setvalue(lua_State* L, int local, char const* str);
int getvalue(lua_State* L, int local, char const* str);
bool islvalue(int code);

String formatstr(char const* str, int len);
WideString towstring(lua_State* L, int pos, bool inctype = false);

}

#endif // __LUA_EVAL__
