#ifndef __ILUA_STREAM__
#define __ILUA_STREAM__

#include "ilua.h"
#include <string.h>
#include <intrin.h>

typedef long long int64;

namespace ilua
{

class Stream : public Object
{
public:
  virtual char getc() {char c = 0; read(&c, 1); return c;}
  virtual int putc(char c) {return write(&c, 1);}

  virtual int read(void* buf, int count) {return 0;}
  virtual int write(void const* buf, int count) {return 0;}

  virtual void seek(int64 pos, int rel) {}
  virtual int64 tell() const {return 0;}

  virtual int64 size() const {return 0;}

  virtual bool eof() const {return tell() >= size();}
  virtual void resize(int64 newsize) {}

  virtual void flush() {}

  unsigned short read16(bool big = false)
  {
    int res = 0;
    read(&res, sizeof res);
    if (big) res = _byteswap_ushort(res);
    return res;
  }
  unsigned long read32(bool big = false)
  {
    int res = 0;
    read(&res, sizeof res);
    if (big) res = _byteswap_ulong(res);
    return res;
  }
  int readstr(char* str, int size)
  {
    int count = 0;
    while (count < size && (*str++ = getc()))
      count++;
    return count;
  }
  void write16(unsigned short n, bool big = false)
  {
    if (big) n = _byteswap_ushort(n);
    write(&n, 2);
  }
  void write32(unsigned long n, bool big = false)
  {
    if (big) n = _byteswap_ulong(n);
    write(&n, 4);
  }
  void writestr(char const* str)
  {
    write(str, strlen(str) + 1);
  }

  float readfloat(bool big = false)
  {
    unsigned long res = read32(big);
    return *(float*) &res;
  }
  double readdouble(bool big = false)
  {
    unsigned long long res = 0;
    read(&res, 8);
    if (big) res = _byteswap_uint64(res);
    return *(double*) &res;
  }
  void writefloat(float f, bool big = false)
  {
    write32(*(unsigned long*) &f, big);
  }
  void writedouble(double f, bool big = false)
  {
    unsigned long long res = *(unsigned long long*) &f;
    if (big) res = _byteswap_uint64(res);
    write(&res, 8);
  }

  void serialize(lua_State* L, int index);
  void deserialize(lua_State* L);

  virtual int64 copy(Stream* stream, int64 count = 0);

  virtual const char* tolstring(size_t* len)
  {
    return NULL;
  }
};

// remove seek, read or write methods from metatable
void stream_noseek(lua_State* L);
void stream_noread(lua_State* L);
void stream_nowrite(lua_State* L);

int isbuffer(lua_State* L, int index);
char const* tobuffer(lua_State* L, int index, size_t* len);
char const* checkbuffer(lua_State* L, int index, size_t* len);
char const* optbuffer(lua_State* L, int index, char const* d, size_t* len);

// create a buffer on stack; position is set to beginning
void pushbuffer(lua_State* L, void const* data, size_t length);

}

#endif // __ILUA_STREAM__
