#ifndef __MATH_RAND__
#define __MATH_RAND__

#include "base/types.h"
#include <lua/lua.hpp>
#include "hash.h"
#include "base/thread.h"

struct RandomState
{
  thread::Lock lock;
  HashState* hash;
  uint32 reseed_counter;
  uint32 seedlen;
  uint8* value;
  uint8* constant;
  uint8* buffer;
  uint8* data;
};
RandomState* random_init(lua_State* L, int algorithm = HashAlgorithm::SHA256, void const* input = NULL, uint32 input_length = 0);
void random_free(RandomState* state);

void random_seed(RandomState* state, void const* input, uint32 input_length);
void random_preseed(RandomState* state, void const* input, uint32 input_length);
void random_get(RandomState* state, void* output, uint32 output_length, void const* input = NULL, uint32 input_length = 0);

void random_bind(lua_State* L);
RandomState* random_default(lua_State* L);
RandomState* random_check(lua_State* L, int* index);
int random_optextra(RandomState* state, lua_State* L, int index);

#endif // __MATH_RAND__
