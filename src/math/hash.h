#ifndef __MATH_HASH__
#define __MATH_HASH__

#include "base/types.h"
#include <lua/lua.hpp>

struct HashAlgorithm
{
  enum {
    CRC32 = 1, MD5, SHA0, SHA1,
    SHA224, SHA256, SHA384, SHA512, SHA512_224, SHA512_256,
    KECCAK224, KECCAK256, KECCAK384, KECCAK512,

    FIRST = CRC32,
    LAST = KECCAK512,
  };
  uint32 blockSize;
  uint32 outputSize;
  uint32 stateSize;
  uint32 padLength;
  uint32 bigWordSize;

  void (*fn_init)(void* state);
  void (*fn_update)(void* state, void const* chunk, uint32 length);
  void (*fn_finish)(void* state, void* output);
};
HashAlgorithm* hash_algorithm(int id);
int hash_algorithm_by_name(char const* name);

struct HashState
{
  HashAlgorithm* algorithm;
  uint8* state;
  uint8* buffer;
  uint64 length;
  uint32 count;
};
uint32 hash_space(int algorithm);
void hash_init_inplace(HashState* state, int algorithm);
HashState* hash_init(lua_State* L, int algorithm);
void hash_free(HashState* state);

void hash_start(HashState* state);
void hash_update(HashState* state, void const* message, uint32 length);
void hash_finish(HashState* state, void* output);

void hash_bind(lua_State* L);

#endif // __MATH_HASH__
