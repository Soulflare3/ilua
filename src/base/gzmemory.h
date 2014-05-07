#ifndef __BASE_GZMEMORY__
#define __BASE_GZMEMORY__

#include <stdlib.h>
#include <memory.h>
#include "base/types.h"

struct gzmemory
{
  int count;
  int pos;
  char* buf;
  int size;
  void reset();
  void* alloc(int block);
  void free(void* ptr);
  gzmemory()
  {
    buf = NULL;
    size = 0;
    count = 65536;
  }
  ~gzmemory()
  {
    if (buf)
      ::free(buf);
  }
};

void* gzalloc(void* param, unsigned int items, unsigned int size);
void* gzalloci(void* param, int items, int size);
void gzfree(void* param, void* ptr);

uint32 gzdeflate (uint8* in, uint32 in_size, uint8* out, uint32* out_size);
uint32 gzinflate (uint8* in, uint32 in_size, uint8* out, uint32* out_size);

#endif // __BASE_GZMEMORY__
