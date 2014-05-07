#include <lua/lua.hpp>
#include "base/types.h"
#include "ilua/ilua.h"
#include "ilua/stream.h"

namespace api
{

class Buffer : public ilua::Stream
{
  int alloc_size;
public:
  int m_size;
  int m_pos;
  char* m_data;

  static size_t buf_size(int sz)
  {
    if (sz < 65536)
    {
      size_t res = 1;
      while (res < sz) res <<= 1;
      return res;
    }
    return (sz + 65535) & (~65535);
  }

  Buffer(int sz = 64)
    : m_size(0)
    , m_pos(0)
    , alloc_size(buf_size(sz))
  {
    m_data = new char[alloc_size];
  }
  ~Buffer()
  {
    delete[] m_data;
  }

  void realloc(int sz)
  {
    if (sz <= alloc_size) return;
    alloc_size = buf_size(sz);
    char* temp = new char[alloc_size];
    memcpy(temp, m_data, m_size);
    delete[] m_data;
    m_data = temp;
  }

  char getc()
  {
    return (m_pos < m_size ? m_data[m_pos++] : 0);
  }
  int putc(char c)
  {
    realloc(m_pos + 1);
    m_data[m_pos++] = c;
    if (m_pos > m_size)
      m_size = m_pos;
    return 1;
  }

  int read(void* buf, int count)
  {
    if (count > m_size - m_pos)
      count = m_size - m_pos;
    memcpy(buf, m_data + m_pos, count);
    m_pos += count;
    return count;
  }
  int write(void const* buf, int count)
  {
    realloc(m_pos + count);
    memcpy(m_data + m_pos, buf, count);
    m_pos += count;
    if (m_pos > m_size)
      m_size = m_pos;
    return count;
  }

  void seek(int64 pos, int rel)
  {
    switch (rel)
    {
    case SEEK_SET:
      m_pos = pos;
      break;
    case SEEK_CUR:
      m_pos += pos;
      break;
    case SEEK_END:
      m_pos = m_size + pos;
      break;
    }
    if (m_pos < 0) m_pos = 0;
    if (m_pos > m_size) m_pos = m_size;
  }
  int64 tell() const
  {
    return m_pos;
  }
  int64 size() const
  {
    return m_size;
  }

  bool eof() const
  {
    return m_pos >= m_size;
  }
  void resize(int64 newsize)
  {
    realloc(newsize);
    if (newsize > m_size)
      memset(m_data + m_size, 0, newsize - m_size);
    m_size = newsize;
  }

  int64 copy(Stream* stream, int64 count)
  {
    if (count == 0)
      count = stream->size() - stream->tell();
    realloc(m_pos + count);
    count = stream->read(m_data + m_pos, count);
    m_pos += count;
    if (m_pos > m_size)
      m_size = m_pos;
    return count;
  }

  const char* tolstring(size_t* len)
  {
    if (len) *len = m_size;
    return m_data;
  }
};

static int stream_read(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  int n = lua_gettop(L);
  for (int i = 2; i <= n; i++)
  {
    int count = luaL_checkinteger(L, i);
    luaL_Buffer b;
    char* p = luaL_buffinitsize(L, &b, count);
    int got = stream->read(p, count);
    luaL_pushresultsize(&b, got);
    if (got == 0)
    {
      lua_pop(L, 1);
      lua_pushnil(L);
    }
  }
  return n - 1;
}
static int stream_write(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  int n = lua_gettop(L);
  for (int i = 2; i <= n; i++)
  {
    size_t count;
    char const* str = ilua::checkbuffer(L, i, &count);
    stream->write(str, count);
  }
  return 0;
}
static int stream_seek(lua_State* L)
{
  static int modes[] = {SEEK_SET, SEEK_CUR, SEEK_END};
  static char const* modenames[] = {"set", "cur", "end", NULL};

  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  int64 offset = int64(luaL_checknumber(L, 2));
  int op = ilua::checkoption(L, 3, "set", modenames);
  stream->seek(offset, modes[op]);
  return 0;
}
static int stream_tell(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  lua_pushnumber(L, (lua_Number) stream->tell());
  return 1;
}
static int stream_size(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  lua_pushnumber(L, (lua_Number) stream->size());
  return 1;
}
static int stream_resize(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  stream->resize((int64) luaL_checknumber(L, 2));
  return 0;
}
static int stream_eof(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  lua_pushboolean(L, stream->eof());
  return 1;
}

static int stream_read8(lua_State* L)
{
  ilua::Stream* stream = (ilua::Stream*) lua_touserdata(L, 1);
  if (stream->eof())
    lua_pushnil(L);
  else
    lua_pushinteger(L, stream->getc());
  return 1;
}
static int stream_read16(lua_State* L)
{
  ilua::Stream* stream = (ilua::Stream*) lua_touserdata(L, 1);
  unsigned short res;
  if (stream->read(&res, sizeof res) != sizeof res)
    lua_pushnil(L);
  else
  {
    if (lua_toboolean(L, 2)) res = _byteswap_ushort(res);
    lua_pushinteger(L, res);
  }
  return 1;
}
static int stream_read32(lua_State* L)
{
  ilua::Stream* stream = (ilua::Stream*) lua_touserdata(L, 1);
  unsigned long res;
  if (stream->read(&res, sizeof res) != sizeof res)
    lua_pushnil(L);
  else
  {
    if (lua_toboolean(L, 2)) res = _byteswap_ulong(res);
    lua_pushinteger(L, res);
  }
  return 1;
}
static int stream_readfloat(lua_State* L)
{
  ilua::Stream* stream = (ilua::Stream*) lua_touserdata(L, 1);
  unsigned long res;
  if (stream->read(&res, sizeof res) != sizeof res)
    lua_pushnil(L);
  else
  {
    if (lua_toboolean(L, 2)) res = _byteswap_ulong(res);
    lua_pushnumber(L, lua_Number(*(float*) &res));
  }
  return 1;
}
static int stream_readdouble(lua_State* L)
{
  ilua::Stream* stream = (ilua::Stream*) lua_touserdata(L, 1);
  unsigned long long res;
  if (stream->read(&res, sizeof res) != sizeof res)
    lua_pushnil(L);
  else
  {
    if (lua_toboolean(L, 2)) res = _byteswap_uint64(res);
    lua_pushnumber(L, lua_Number(*(double*) &res));
  }
  return 1;
}
static int stream_readstr(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while (int c = stream->getc())
    luaL_addchar(&b, c);
  luaL_pushresult(&b);
  return 1;
}
static int stream_readbuf(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  size_t size = stream->read32();
  Buffer* buffer = new(L, "buffer") Buffer(size);
  buffer->m_pos = buffer->m_size = stream->read(buffer->m_data, size);
  return 1;
}
static int stream_write8(lua_State* L)
{
  ilua::Stream* stream = (ilua::Stream*) lua_touserdata(L, 1);
  stream->putc(luaL_checkinteger(L, 2));
  return 0;
}
static int stream_write16(lua_State* L)
{
  ilua::Stream* stream = (ilua::Stream*) lua_touserdata(L, 1);
  stream->write16(luaL_checkinteger(L, 2), lua_toboolean(L, 3));
  return 0;
}
static int stream_write32(lua_State* L)
{
  ilua::Stream* stream = (ilua::Stream*) lua_touserdata(L, 1);
  stream->write32(luaL_checkinteger(L, 2), lua_toboolean(L, 3));
  return 0;
}
static int stream_writefloat(lua_State* L)
{
  ilua::Stream* stream = (ilua::Stream*) lua_touserdata(L, 1);
  stream->writefloat(luaL_checknumber(L, 2), lua_toboolean(L, 3));
  return 0;
}
static int stream_writedouble(lua_State* L)
{
  ilua::Stream* stream = (ilua::Stream*) lua_touserdata(L, 1);
  stream->writedouble(luaL_checknumber(L, 2), lua_toboolean(L, 3));
  return 0;
}
static int stream_writestr(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  stream->writestr(luaL_checkstring(L, 2));
  return 0;
}
static int stream_writebuf(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  size_t length;
  char const* str = ilua::checkbuffer(L, 2, &length);
  stream->write32(length);
  stream->write(str, length);
  return 0;
}

static Buffer* buf_get(lua_State* L, int idx)
{
  if (Buffer* buf = ilua::toobject<Buffer>(L, idx, "buffer"))
    return buf;
  if (lua_isnoneornil(L, idx))
  {
    Buffer* buf = new(L, "buffer") Buffer();
    if (lua_gettop(L) > idx)
      lua_replace(L, idx);
    return buf;
  }
  size_t length;
  char const* ptr = ilua::tobuffer(L, idx, &length);
  if (ptr == NULL)
    luaL_argerror(L, idx, "buffer expected");
  Buffer* buf = new(L, "buffer") Buffer(length);
  buf->write(ptr, length);
  lua_replace(L, idx);
  return buf;
}

static int buf_create(lua_State* L)
{
  buf_get(L, 1);
  lua_settop(L, 1);
  return 1;
}
static int buf_tostring(lua_State* L)
{
  Buffer* buf = buf_get(L, 1);
  lua_pushlstring(L, buf->m_data, buf->m_size);
  return 1;
}
static int buf_concat(lua_State* L)
{
  size_t len1, len2;
  char const* buf1 = ilua::checkbuffer(L, 1, &len1);
  char const* buf2 = ilua::checkbuffer(L, 1, &len2);
  Buffer* buf = new(L, "buffer") Buffer(len1 + len2);
  buf->write(buf1, len1);
  buf->write(buf2, len2);
  return 1;
}
static int buf_length(lua_State* L)
{
  size_t len;
  char const* buf = ilua::checkbuffer(L, 1, &len);
  lua_pushinteger(L, len);
  return 1;
}

#define RAW_TNIL            0         // 0 bytes
#define RAW_TBOOLEAN        1         // 1 byte
#define RAW_TNUMBER         3         // 8 bytes
#define RAW_TSTRING         4         // 4 bytes + N
#define RAW_TTABLE          5         // 4 bytes + N pairs of <var> + (<var> metatable)
#define RAW_TFUNCTION       6         // 4 bytes + N
#define RAW_TBUFFER         0x0F      // 4 bytes + N
#define RAW_HASMETATABLE    0x80U
#define RAW_TREPEAT         0xFFU

static int stream_writer(lua_State* L, const void* p, size_t sz, void* ud)
{
  ((ilua::Stream*) ud)->write(p, sz);
  return 0;
}
static int buf_writer(lua_State* L, const void* p, size_t sz, void* ud)
{
  luaL_addlstring((luaL_Buffer*) ud, (const char*) p, sz);
  return 0;
}
struct stream_reader_info
{
  ilua::Stream* stream;
  uint32 remains;
  char buf[BUFSIZ];
};
static const char* stream_reader(lua_State* L, void* data, size_t* size)
{
  stream_reader_info* info = (stream_reader_info*) data;
  if (info->remains == 0)
    return NULL;
  *size = BUFSIZ;
  if (*size > info->remains)
    *size = info->remains;
  *size = info->stream->read(info->buf, *size);
  if (*size == 0)
    return NULL;
  info->remains -= *size;
  return info->buf;
}

static void do_serialize(ilua::Stream* stream, lua_State* L, int index, int cache)
{
  switch (lua_type(L, index))
  {
  case LUA_TNIL:
    stream->putc(RAW_TNIL);
    break;
  case LUA_TBOOLEAN:
    stream->putc(RAW_TBOOLEAN);
    stream->putc(lua_toboolean(L, index));
    break;
  case LUA_TNUMBER:
    {
      stream->putc(RAW_TNUMBER);
      lua_Number num = lua_tonumber(L, index);
      stream->write(&num, 8);
    }
    break;
  case LUA_TSTRING:
    {
      stream->putc(RAW_TSTRING);
      size_t len;
      char const* ptr = lua_tolstring(L, index, &len);
      stream->write32(len);
      stream->write(ptr, len);
    }
    break;
  case LUA_TTABLE:
    {
      lua_pushvalue(L, index);
      lua_rawget(L, cache);
      if (lua_isnumber(L, -1))
      {
        stream->putc(RAW_TREPEAT);
        stream->write32(stream->tell() - (int64) lua_tonumber(L, -1));
        lua_pop(L, 1);
      }
      else
      {
        int type = RAW_TTABLE;
        if (lua_getmetatable(L, index))
        {
          lua_pop(L, 1);
          type |= RAW_HASMETATABLE;
        }
        stream->putc(type);
        lua_pop(L, 1);
        lua_pushvalue(L, index);
        lua_pushnumber(L, (lua_Number) stream->tell());
        lua_rawset(L, cache);
        uint32 count = 0;
        lua_pushnil(L);
        while (lua_next(L, index))
        {
          count++;
          lua_pop(L, 1);
        }
        stream->write32(count);
        lua_pushnil(L);
        while (lua_next(L, index))
        {
          do_serialize(stream, L, lua_gettop(L) - 1, cache);
          do_serialize(stream, L, lua_gettop(L), cache);
          lua_pop(L, 1);
        }
        if (lua_getmetatable(L, index))
        {
          do_serialize(stream, L, lua_gettop(L), cache);
          lua_pop(L, 1);
        }
      }
    }
    break;
  case LUA_TFUNCTION:
    if (lua_iscfunction(L, index))
      luaL_error(L, "unable to serialize C function");
    else
    {
      lua_pushvalue(L, index);
      lua_rawget(L, cache);
      if (lua_isnumber(L, -1))
      {
        stream->putc(RAW_TREPEAT);
        stream->write32(stream->tell() - (int64) lua_tonumber(L, -1));
        lua_pop(L, 1);
      }
      else
      {
        stream->putc(RAW_TFUNCTION);
        lua_pop(L, 1);
        lua_pushvalue(L, index);
        lua_pushnumber(L, (lua_Number) stream->tell());
        lua_rawset(L, cache);
        lua_pushvalue(L, index);
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        int code = lua_dump(L, buf_writer, &b);
        luaL_pushresult(&b);
        if (code == 0)
        {
          size_t length;
          char const* ptr = lua_tolstring(L, -1, &length);
          stream->write32(length);
          stream->write(ptr, length);
        }
        else
          stream->write32(0);
        lua_pop(L, 2);
      }
    }
    break;
  case LUA_TUSERDATA:
    if (Buffer* buf = ilua::toobject<Buffer>(L, index, "buffer"))
    {
      stream->putc(RAW_TBUFFER);
      stream->write32(buf->m_size);
      stream->write(buf->m_data, buf->m_size);
      break;
    }
  default:
    luaL_error(L, "unable to serialize %s", lua_typename(L, lua_type(L, index)));
  }
}
static void do_deserialize(ilua::Stream* stream, lua_State* L, int cache)
{
  if (stream->eof())
  {
    lua_pushnil(L);
    return;
  }
  uint8 type = stream->getc();
  switch (type)
  {
  case RAW_TNIL:
    lua_pushnil(L);
    break;
  case RAW_TBOOLEAN:
    if (stream->eof())
      lua_pushnil(L);
    else
      lua_pushboolean(L, stream->getc());
    break;
  case RAW_TNUMBER:
    {
      lua_Number val;
      if (stream->read(&val, 8) == 8)
        lua_pushnumber(L, val);
      else
        lua_pushnil(L);
    }
    break;
  case RAW_TSTRING:
    {
      size_t count;
      if (stream->read(&count, 4) == 4)
      {
        luaL_Buffer b;
        char* p = luaL_buffinitsize(L, &b, count);
        int got = stream->read(p, count);
        luaL_pushresultsize(&b, got);
        if (got != count)
        {
          lua_pop(L, 1);
          lua_pushnil(L);
        }
      }
      else
        lua_pushnil(L);
    }
    break;
  case RAW_TTABLE:
  case RAW_TTABLE | RAW_HASMETATABLE:
    {
      int64 offset = stream->tell();
      uint32 count;
      if (stream->read(&count, 4) != 4)
      {
        lua_pushnil(L);
        break;
      }
      lua_newtable(L);
      int index = lua_gettop(L);
      for (uint32 i = 0; i < count && !stream->eof(); i++)
      {
        do_deserialize(stream, L, cache);
        do_deserialize(stream, L, cache);
        lua_rawset(L, index);
      }
      if ((type & RAW_HASMETATABLE) && !stream->eof())
      {
        do_deserialize(stream, L, cache);
        lua_setmetatable(L, index);
      }
      lua_pushvalue(L, index);
      lua_rawseti(L, cache, offset);
    }
    break;
  case RAW_TFUNCTION:
    {
      int64 offset = stream->tell();
      stream_reader_info info;
      info.stream = stream;
      if (stream->read(&info.remains, 4) != 4 || lua_load(L, stream_reader, &info, "=(stream)", "b") != LUA_OK)
      {
        lua_pop(L, 1);
        lua_pushnil(L);
      }
      lua_pushvalue(L, -1);
      lua_rawseti(L, cache, offset);
    }
    break;
  case RAW_TREPEAT:
    {
      uint32 offset;
      int64 pos = stream->tell();
      if (stream->read(&offset, 4) != 4)
        lua_pushnil(L);
      else
        lua_rawgeti(L, cache, pos - offset);
    }
    break;
  }
}

static int stream_serialize(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  int n = lua_gettop(L);
  lua_newtable(L);
  for (int i = 2; i <= n; i++)
    do_serialize(stream, L, i, n + 1);
  lua_settop(L, 1);
  return 1;
}
static int stream_deserialize(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  int count = luaL_optint(L, 2, 1);
  lua_newtable(L);
  int cache = lua_gettop(L);
  for (int i = 0; i < count; i++)
    do_deserialize(stream, L, cache);
  return count;
}

static int stream_copy(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  ilua::Stream* from = ilua::checkobject<ilua::Stream>(L, 2, "stream");
  stream->copy(from, luaL_optinteger(L, 3, 0));
  return 0;
}
static int stream_printf(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  int nargs = lua_gettop(L) - 1;
  lua_getglobal(L, "string");
  lua_getfield(L, -1, "format");
  lua_insert(L, 2);
  lua_pop(L, 1);
  ilua::lcall(L, nargs, 1);
  size_t length;
  char const* str = lua_tolstring(L, 2, &length);
  stream->write(str, length);
  return 0;
}

static int stream_getline(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  int count = 0;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while (int c = stream->getc())
  {
    count++;
    if (c != '\r' && c != '\n')
      luaL_addchar(&b, c);
    else
    {
      int pos = stream->tell();
      int nc = stream->getc();
      if ((nc != '\r' && nc != '\n') || nc == c)
        stream->seek(pos, SEEK_SET);
      break;
    }
  }
  luaL_pushresult(&b);
  if (count == 0)
  {
    lua_pop(L, 1);
    lua_pushnil(L);
  }
  return 1;
}
static int stream_lines(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  lua_pushcfunction(L, stream_getline);
  lua_pushvalue(L, 1);
  lua_pushnil(L);
  return 3;
}
static int stream_flush(lua_State* L)
{
  ilua::Stream* stream = ilua::checkobject<ilua::Stream>(L, 1, "stream");
  stream->flush();
  return 0;
}

static void push_buffer(lua_State* L, void const* data, size_t length)
{
  Buffer* buf = new(L, "buffer") Buffer(length);
  memcpy(buf->m_data, data, length);
  buf->m_size = length;
}

void bind_stream(lua_State* L)
{
  ilua::newtype<ilua::Stream>(L, "stream");
  ilua::bindmethod(L, "read", stream_read);
  ilua::bindmethod(L, "write", stream_write);
  ilua::bindmethod(L, "seek", stream_seek);
  ilua::bindmethod(L, "tell", stream_tell);
  ilua::bindmethod(L, "eof", stream_eof);
  ilua::bindmethod(L, "size", stream_size);
  ilua::bindmethod(L, "resize", stream_resize);
  ilua::bindmethod(L, "read8", stream_read8);
  ilua::bindmethod(L, "read16", stream_read16);
  ilua::bindmethod(L, "read32", stream_read32);
  ilua::bindmethod(L, "readfloat", stream_readfloat);
  ilua::bindmethod(L, "readdouble", stream_readdouble);
  ilua::bindmethod(L, "readstr", stream_readstr);
  ilua::bindmethod(L, "readbuf", stream_readbuf);
  ilua::bindmethod(L, "write8", stream_write8);
  ilua::bindmethod(L, "write16", stream_write16);
  ilua::bindmethod(L, "write32", stream_write32);
  ilua::bindmethod(L, "writefloat", stream_writefloat);
  ilua::bindmethod(L, "writedouble", stream_writedouble);
  ilua::bindmethod(L, "writestr", stream_writestr);
  ilua::bindmethod(L, "writebuf", stream_writebuf);
  ilua::bindmethod(L, "serialize", stream_serialize);
  ilua::bindmethod(L, "deserialize", stream_deserialize);
  ilua::bindmethod(L, "copy", stream_copy);
  ilua::bindmethod(L, "printf", stream_printf);
  ilua::bindmethod(L, "getline", stream_getline);
  ilua::bindmethod(L, "lines", stream_lines);
  ilua::bindmethod(L, "flush", stream_flush);
  lua_pop(L, 2);

  ilua::newtype<Buffer>(L, "buffer", "stream");
  ilua::bindmethod(L, "tostring", buf_tostring);
  lua_pop(L, 1);
  ilua::bindmethod(L, "__tostring", buf_tostring);
  ilua::bindmethod(L, "__concat", buf_concat);
  ilua::bindmethod(L, "__len", buf_length);
  lua_pop(L, 1);

  ilua::openlib(L, "buffer");
  ilua::bindmethod(L, "create", buf_create);
  lua_pop(L, 1);

  lua_pushlightuserdata(L, push_buffer);
  lua_setfield(L, LUA_REGISTRYINDEX, "ilua_push_buffer");
}

}
