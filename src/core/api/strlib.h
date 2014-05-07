#ifndef __CORE_API_STRLIB__
#define __CORE_API_STRLIB__

#include <lua/lua.hpp>

namespace api
{

void bind_utf8(lua_State* L);
void bind_re(lua_State* L);

}

#endif // __CORE_API_STRLIB__
