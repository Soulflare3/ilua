#ifndef __BASE_UTF8__
#define __BASE_UTF8__

#include "base/types.h"

namespace utf8
{

extern uint32 tf_lower[256];
extern uint32 tf_upper[256];

uint32 transform(uint8_const_ptr* ptr, uint32* table);
inline uint32 transform(uint8_const_ptr ptr, uint32* table)
{
  return transform(&ptr, table);
}
uint8_const_ptr next(uint8_const_ptr ptr);

uint32 parse(uint32 cp);

}

#endif // __BASE_UTF8__
