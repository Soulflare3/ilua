#include "stream.h"

namespace ilua
{

int64 Stream::copy(Stream* stream, int64 count)
{
  static unsigned char buf[4096];
  int64 done = 0;
  while (count == 0 || done < count)
  {
    int64 to = sizeof buf;
    if (count != 0 && done + to > count)
      to = count - done;
    to = stream->read(buf, to);
    if (to == 0)
      break;
    write(buf, to);
    done += to;
  }
  return done;
}
void Stream::serialize(lua_State* L, int index)
{
  index = lua_absindex(L, index);
  ilua::pushobject(L, this);
  lua_getfield(L, -1, "serialize");
  lua_pushvalue(L, -2);
  lua_pushvalue(L, index);
  ilua::lcall(L, 2, 0);
  lua_pop(L, 1);
}
void Stream::deserialize(lua_State* L)
{
  ilua::pushobject(L, this);
  lua_getfield(L, -1, "deserialize");
  lua_pushvalue(L, -2);
  ilua::lcall(L, 1, 0);
  lua_pop(L, 1);
}

int isbuffer(lua_State* L, int index)
{
  return (iskindof(L, index, "stream") || lua_isstring(L, index));
}
char const* tobuffer(lua_State* L, int index, size_t* len)
{
  if (Stream* stream = toobject<Stream>(L, index, "stream"))
    return stream->tolstring(len);
  return lua_tolstring(L, index, len);
}
char const* checkbuffer(lua_State* L, int index, size_t* len)
{
  if (Stream* stream = toobject<Stream>(L, index, "stream"))
  {
    char const* ret = stream->tolstring(len);
    if (ret) return ret;
  }
  return luaL_checklstring(L, index, len);
}
char const* optbuffer(lua_State* L, int index, char const* d, size_t* len)
{
  if (Stream* stream = toobject<Stream>(L, index, "stream"))
  {
    char const* ret = stream->tolstring(len);
    if (ret) return ret;
  }
  return luaL_optlstring(L, index, d, len);
}

typedef void (*push_buffer_type)(lua_State*, void const*, size_t);
void pushbuffer(lua_State* L, void const* data, size_t length)
{
  lua_getfield(L, LUA_REGISTRYINDEX, "ilua_push_buffer");
  push_buffer_type pf = (push_buffer_type) lua_touserdata(L, -1);
  lua_pop(L, 1);
  if (pf)
    pf(L, data, length);
  else
    lua_pushnil(L);
}

void stream_noseek(lua_State* L)
{
  ilua::settabsn(L, "seek");
  ilua::settabsn(L, "tell");
  ilua::settabsn(L, "eof");
  ilua::settabsn(L, "resize");
  ilua::settabsn(L, "size");
}
void stream_noread(lua_State* L)
{
  ilua::settabsn(L, "read");
  ilua::settabsn(L, "read8");
  ilua::settabsn(L, "read16");
  ilua::settabsn(L, "read32");
  ilua::settabsn(L, "readfloat");
  ilua::settabsn(L, "readdouble");
  ilua::settabsn(L, "readstr");
  ilua::settabsn(L, "readbuf");
  ilua::settabsn(L, "deserialize");
  ilua::settabsn(L, "getline");
  ilua::settabsn(L, "lines");
}
void stream_nowrite(lua_State* L)
{
  ilua::settabsn(L, "write");
  ilua::settabsn(L, "resize");
  ilua::settabsn(L, "write8");
  ilua::settabsn(L, "write16");
  ilua::settabsn(L, "write32");
  ilua::settabsn(L, "writefloat");
  ilua::settabsn(L, "writedouble");
  ilua::settabsn(L, "writestr");
  ilua::settabsn(L, "writebuf");
  ilua::settabsn(L, "serialize");
  ilua::settabsn(L, "copy");
  ilua::settabsn(L, "printf");
  ilua::settabsn(L, "flush");
}

}
