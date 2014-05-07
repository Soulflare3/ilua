#include "gzmemory.h"
#include "zlib/zlib.h"

void gzmemory::reset()
{
  if (count > size)
  {
    if (buf)
      ::free(buf);
    buf = (char*) malloc(count);
    size = count;
  }
  count = 0;
  pos = 0;
}
void* gzmemory::alloc(int block)
{
  if (this == NULL)
    return malloc(block);
  count += block;
  if (pos + block > size)
    return malloc(block);
  char* ptr = buf + pos;
  pos += block;
  return ptr;
}
void gzmemory::free(void* ptr)
{
  if (this == NULL || (ptr && (ptr < buf || ptr >= buf + size)))
    ::free(ptr);
}
void* gzalloc(void* param, unsigned int items, unsigned int size)
{
  if (param == NULL) return malloc(items * size);
  return ((gzmemory*) param)->alloc(items * size);
}
void* gzalloci(void* param, int items, int size)
{
  if (param == NULL) return malloc(items * size);
  return ((gzmemory*) param)->alloc(items * size);
}
void gzfree(void* param, void* ptr)
{
  if (param) ((gzmemory*) param)->free (ptr);
  else free(ptr);
}

uint32 gzdeflate (uint8* in, uint32 in_size, uint8* out, uint32* out_size)
{
  z_stream z;
  memset (&z, 0, sizeof z);
  z.next_in = in;
  z.avail_in = in_size;
  z.total_in = in_size;
  z.next_out = out;
  z.avail_out = *out_size;
  z.total_out = 0;

  memset (out, 0, *out_size);

  int result = deflateInit (&z, Z_DEFAULT_COMPRESSION);
  if (result == Z_OK)
  {
    result = deflate (&z, Z_FINISH);
    *out_size = z.total_out;
    deflateEnd (&z);
  }
  return (result == Z_STREAM_END ? 0 : -1);
}
uint32 gzinflate (uint8* in, uint32 in_size, uint8* out, uint32* out_size)
{
  z_stream z;
  memset (&z, 0, sizeof z);
  z.next_in = in;
  z.avail_in = in_size;
  z.total_in = in_size;
  z.next_out = out;
  z.avail_out = *out_size;
  z.total_out = 0;

  memset (out, 0, *out_size);

  int result = inflateInit (&z);
  if (result == Z_OK)
  {
    result = inflate (&z, Z_FINISH);
    *out_size = z.total_out;
    inflateEnd (&z);
  }
  return (z.avail_out == 0 ? 0 : -1);
}
