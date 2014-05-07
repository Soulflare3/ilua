#include "rand.h"
#include <math.h>
#include <stdlib.h>
#include "ilua/ilua.h"

struct hash_input_list
{
  void const* input;
  uint32 input_length;
};

static void hash_derivation(HashState* hash, uint8* buffer, uint8* output, uint32 output_length, hash_input_list* input_list, uint32 input_count)
{
  uint8 counter = 1;
  uint32 no_of_bits_to_return = output_length * 8;
  while (output_length)
  {
    hash_start(hash);
    hash_update(hash, &counter, 1);
    hash_update(hash, &no_of_bits_to_return, 4);
    for (uint32 i = 0; i < input_count; i++)
      if (input_list[i].input)
        hash_update(hash, input_list[i].input, input_list[i].input_length);

    uint32 addlen = hash->algorithm->outputSize;
    if (hash->algorithm->outputSize <= output_length)
      hash_finish(hash, output);
    else
    {
      hash_finish(hash, buffer);
      memcpy(output, buffer, output_length);
      addlen = output_length;
    }
    output += addlen;
    output_length -= addlen;

    counter++;
  }
}

RandomState* random_init(lua_State* L, int algorithm, void const* input, uint32 input_length)
{
  HashAlgorithm* alg = hash_algorithm(algorithm);
  if (alg == NULL) return 0;
  uint32 seedlen = 55;
  if (alg->outputSize > 32) seedlen = 111;
  uint32 space = sizeof(RandomState) + hash_space(algorithm) + seedlen * 3 + alg->outputSize;
  RandomState* state;
  if (L)
    state = ilua::newstruct<RandomState>(L, "random.generator", space);
  else
  {
    state = (RandomState*) malloc(space);
    new(state) RandomState;
  }
  state->seedlen = seedlen;
  state->reseed_counter = 1;
  state->value = (uint8*) (state + 1);
  state->constant = (uint8*) (state->value + seedlen);
  state->data = (uint8*) (state->constant + seedlen);
  state->buffer = (uint8*) (state->data + seedlen);
  state->hash = (HashState*) (state->buffer + alg->outputSize);
  hash_init_inplace(state->hash, algorithm);

  uint8 prefix = 0;
  hash_input_list input_list[3] = {
    {input, input_length},
    {&prefix, 1},
    {state->value, seedlen},
  };
  hash_derivation(state->hash, state->buffer, state->value, seedlen, &input_list[0], 1);
  hash_derivation(state->hash, state->buffer, state->constant, seedlen, &input_list[1], 2);

  return state;
}
void random_free(RandomState* state)
{
  free(state);
}

void random_seed(RandomState* state, void const* input, uint32 input_length)
{
  state->lock.acquire();
  uint8 prefix = 1;
  hash_input_list input_list[3] = {
    {&prefix, 1},
    {state->value, state->seedlen},
    {input, input_length},
  };
  hash_derivation(state->hash, state->buffer, state->constant, state->seedlen, &input_list[0], 3);
  memcpy(state->value, state->constant, state->seedlen);
  prefix = 0;
  hash_derivation(state->hash, state->buffer, state->constant, state->seedlen, &input_list[0], 2);
  state->reseed_counter = 1;
  state->lock.release();
}

static void random_add(uint8* dst, uint32 dst_length, uint8 const* src, uint32 src_length)
{
  if (src_length > dst_length) src_length = dst_length;
  uint32 carry = 0;
  uint32 pos = 0;
  while (pos < src_length)
  {
    carry += uint32(dst[pos]) + uint32(src[pos]);
    dst[pos++] = uint8(carry);
    carry >>= 8;
  }
  while (pos < dst_length && carry)
  {
    carry += uint32(dst[pos]);
    dst[pos++] = uint8(carry);
    carry >>= 8;
  }
}
void random_preseed(RandomState* state, void const* input, uint32 input_length)
{
  state->lock.acquire();
  uint8 two = 2;
  hash_start(state->hash);
  hash_update(state->hash, &two, 1);
  hash_update(state->hash, state->value, state->seedlen);
  hash_update(state->hash, input, input_length);
  hash_finish(state->hash, state->buffer);
  random_add(state->value, state->seedlen, state->buffer, state->hash->algorithm->outputSize);
  state->lock.release();
}
void random_get(RandomState* state, void* output, uint32 output_length, void const* input, uint32 input_length)
{
  state->lock.acquire();
  uint8 one = 1;
  uint8 three = 3;
  if (input && input_length)
    random_preseed(state, input, input_length);
  memcpy(state->data, state->value, state->seedlen);
  uint8* outptr = (uint8*) output;
  while (output_length)
  {
    hash_start(state->hash);
    hash_update(state->hash, state->data, state->seedlen);

    uint32 addlen = state->hash->algorithm->outputSize;
    if (state->hash->algorithm->outputSize <= output_length)
      hash_finish(state->hash, outptr);
    else
    {
      hash_finish(state->hash, state->buffer);
      memcpy(outptr, state->buffer, output_length);
      addlen = output_length;
    }
    outptr += addlen;
    output_length -= addlen;

    random_add(state->data, state->seedlen, &one, 1);
  }

  hash_start(state->hash);
  hash_update(state->hash, &three, 1);
  hash_update(state->hash, state->value, state->seedlen);
  hash_finish(state->hash, state->buffer);

  random_add(state->value, state->seedlen, state->buffer, state->hash->algorithm->outputSize);
  random_add(state->value, state->seedlen, state->constant, state->seedlen);
  random_add(state->value, state->seedlen, (uint8*) &state->reseed_counter, 4);
  state->reseed_counter++;
  state->lock.release();
}

RandomState* random_default(lua_State* L)
{
  lua_getglobal(L, "random");
  lua_getfield(L, -1, "default");
  RandomState* state = ilua::toobject<RandomState>(L, -1, "random.generator");
  lua_pop(L, 2);
  if (state == NULL)
    luaL_error(L, "random.default must be of type random.generator");
  return state;
}
RandomState* random_check(lua_State* L, int* index)
{
  RandomState* state = ilua::toobject<RandomState>(L, *index, "random.generator");
  if (state) (*index)++;
  else state = random_default(L);
  return state;
}
int random_optextra(RandomState* state, lua_State* L, int index)
{
  size_t extra_length;
  char const* extra = luaL_optlstring(L, index, NULL, &extra_length);
  if (extra) random_preseed(state, extra, extra_length);
  return extra != NULL;
}
static int rlib_new(lua_State* L)
{
  size_t input_length;
  char const* input = luaL_optlstring(L, 1, NULL, &input_length);
  char const* algname = luaL_optstring(L, 2, "SHA-256");
  int algorithm = hash_algorithm_by_name(algname);
  if (algorithm == 0) luaL_argerror(L, 2, "unknown algorithm");

  random_init(L, algorithm, input, input_length);
  return 1;
}
static int rlib_seed(lua_State* L)
{
  int index = 1;
  RandomState* state = random_check(L, &index);

  size_t input_length;
  char const* input = luaL_checklstring(L, index, &input_length);
  random_seed(state, input, input_length);

  return 0;
}
static int rlib_get(lua_State* L)
{
  int index = 1;
  RandomState* state = random_check(L, &index);

  uint32 output_length = luaL_checkinteger(L, index);
  size_t input_length;
  char const* input = luaL_optlstring(L, index + 1, NULL, &input_length);

  luaL_Buffer b;
  char* output = luaL_buffinitsize(L, &b, output_length);
  random_get(state, output, output_length, input, input_length);
  luaL_pushresultsize(&b, output_length);

  return 1;
}

static double rand_number(RandomState* state)
{
  static uint64 tm53 = 0x3CA0000000000000ULL;
  uint64 x = 0;
  random_get(state, &x, 7, NULL, 0);
  return double(x >> 3) * (*(double*) &tm53);
}

static int rlib_number(lua_State* L)
{
  int index = 1;
  RandomState* state = random_check(L, &index);

  double a = 0;
  double b = luaL_optnumber(L, index, 1);
  if (lua_gettop(L) > index)
  {
    a = b;
    b = luaL_checknumber(L, index + 1);
  }

  lua_pushnumber(L, a + (b - a) * rand_number(state));
  return 1;
}
static int rlib_integer(lua_State* L)
{
  int index = 1;
  RandomState* state = random_check(L, &index);

  int a = 1;
  int b = luaL_checkinteger(L, index);
  if (lua_gettop(L) > index)
  {
    a = b;
    b = luaL_checkinteger(L, index + 1);
  }

  if (a == b)
  {
    lua_pushinteger(L, a);
    return 1;
  }
  if (a > b)
  {
    int t = a;
    a = b;
    b = t;
  }

  uint32 count = b - a;
  uint32 mask = 1;
  while (mask < count)
    mask = (mask << 1) + 1;

  uint32 result;
  do
  {
    random_get(state, &result, sizeof result, NULL, 0);
    result &= mask;
  } while (result > count);

  lua_pushinteger(L, a + result);
  return 1;
}

struct IntGen
{
  RandomState* state;
  uint32 buf[32];
  int size;
  IntGen(RandomState* rs)
    : state(rs)
    , size(0)
  {}

  uint32 get(uint32 range)
  {
    uint32 mask = 1;
    while (mask <= range)
      mask <<= 1;
    uint32 result;
    do
    {
      if (size == 0)
      {
        random_get(state, buf, sizeof buf, NULL, 0);
        size = sizeof buf / sizeof buf[0];
      }
      result = buf[--size] & (mask - 1);
    } while (result >= range);
    return result;
  }
};

static int rlib_choice(lua_State* L)
{
  int index = 1;
  IntGen gen(random_check(L, &index));
  int n = lua_gettop(L);
  lua_settop(L, gen.get(n - index + 1) + index);
  return 1;
}
static int rlib_shuffle(lua_State* L)
{
  int index = 1;
  IntGen gen(random_check(L, &index));
  int n = lua_gettop(L);
  for (int k = n; k >= index; k--)
  {
    int pos = gen.get(k - index + 1) + index;
    lua_pushvalue(L, pos);
    if (pos < k)
    {
      lua_pushvalue(L, k);
      lua_replace(L, pos);
    }
  }
  return lua_gettop(L) - n;
}
static int rlib_sample(lua_State* L)
{
  int index = 1;
  IntGen gen(random_check(L, &index));
  int m = luaL_checkinteger(L, index++);
  int n = lua_gettop(L);
  for (int k = n, i = 0; k >= index && i < m; k--, i++)
  {
    int pos = gen.get(k - index + 1) + index;
    lua_pushvalue(L, pos);
    if (pos < k)
    {
      lua_pushvalue(L, k);
      lua_replace(L, pos);
    }
  }
  return lua_gettop(L) - n;
}

static int rlib_triangular(lua_State* L)
{
  int index = 1;
  RandomState* state = random_check(L, &index);

  double a = 0;
  double b = luaL_optnumber(L, index, 1);
  double c = 0.5;
  if (lua_gettop(L) > index)
    a = b, b = luaL_checknumber(L, ++index);
  if (lua_gettop(L) > index)
    c = (luaL_checknumber(L, ++index) - a) / (b - a);

  double u = rand_number(state);
  if (u > c)
  {
    u = 1 - u;
    c = 1 - c;
    double t = a;
    a = b;
    b = t;
  }
  lua_pushnumber(L, a + (b - a) * sqrt(u * c));
  return 1;
}
static double normalvariate(RandomState* state)
{
  static double magic = 1.7155277699214135929603792825575;
  double x, y, z;
  do
  {
    x = rand_number(state);
    y = 1 - rand_number(state);
    z = magic * (x - 0.5) / y;
  } while(z * z > -4 * log(y));
  return z;
}
static int rlib_normalvariate(lua_State* L)
{
  int index = 1;
  RandomState* state = random_check(L, &index);

  double mu = luaL_optnumber(L, index, 0);
  double sigma = luaL_optnumber(L, index + 1, 1);
  lua_pushnumber(L, mu + normalvariate(state) * sigma);
  return 1;
}
static int rlib_lognormvariate(lua_State* L)
{
  int index = 1;
  RandomState* state = random_check(L, &index);

  double mu = luaL_optnumber(L, index, 0);
  double sigma = luaL_optnumber(L, index + 1, 1);
  lua_pushnumber(L, exp(mu + normalvariate(state) * sigma));
  return 1;
}
static int rlib_expovariate(lua_State* L)
{
  int index = 1;
  RandomState* state = random_check(L, &index);
  double lambda = luaL_optnumber(L, index, 1);
  lua_pushnumber(L, -log(1 - rand_number(state)) / lambda);
  return 1;
}

static void bindmethods(lua_State* L)
{
  ilua::bindmethod(L, "seed", rlib_seed);
  ilua::bindmethod(L, "get", rlib_get);

  ilua::bindmethod(L, "number", rlib_number);
  ilua::bindmethod(L, "integer", rlib_integer);
  ilua::bindmethod(L, "choice", rlib_choice);
  ilua::bindmethod(L, "shuffle", rlib_shuffle);
  ilua::bindmethod(L, "sample", rlib_sample);

  ilua::bindmethod(L, "uniform", rlib_number);
  ilua::bindmethod(L, "triangular", rlib_triangular);
  ilua::bindmethod(L, "normalvariate", rlib_normalvariate);
  ilua::bindmethod(L, "lognormvariate", rlib_lognormvariate);
  ilua::bindmethod(L, "expovariate", rlib_expovariate);
}
void random_bind(lua_State* L)
{
  ilua::newtype<RandomState>(L, "random.generator");
  bindmethods(L);
  lua_pop(L, 2);

  ilua::openlib(L, "random");
  ilua::bindmethod(L, "new", rlib_new);
  bindmethods(L);
  random_init(L);
  lua_setfield(L, -2, "default");
  lua_pop(L, 1);
}
