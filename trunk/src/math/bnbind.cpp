#include "bignum.h"
#include "rand.h"
#include <lua/lua.hpp>
#include "ilua/ilua.h"
#include "ilua/slowop.h"

static bn::Number* bn_create(lua_State* L)
{
  return new(L, "bn.number") bn::Number();
}

static bn::Number* bn_get(lua_State* L, int idx, int base = 0)
{
  idx = lua_absindex(L, idx);
  switch (lua_type(L, idx))
  {
  case LUA_TNUMBER:
    {
      lua_Number num = lua_tonumber(L, idx);
      bn::Number* r = bn_create(L);
      bn::fromdouble(r, num);
      lua_replace(L, idx);

      return r;
    }
  case LUA_TSTRING:
    {
      size_t length;
      char const* str = lua_tolstring(L, idx, &length);
      bn::Number* r = bn::fromstring(bn_create(L), str, length, base);
      if (r == NULL)
        luaL_argerror(L, idx, "number expected");
      lua_replace(L, idx);

      return r;
    }
  default:
    return ilua::checkobject<bn::Number>(L, idx, "bn.number");
  }
}

static int bn_number(lua_State* L)
{
  bn_get(L, 1, luaL_optinteger(L, 2, 0));
  lua_settop(L, 1);
  return 1;
}
static int bn_tonumber(lua_State* L)
{
  bn::Number* b = bn_get(L, 1);
  lua_pushnumber(L, bn::todouble(b));
  return 1;
}
static int luaStringBuilder(char const* p, size_t sz, void* ud)
{
  luaL_addlstring((luaL_Buffer*) ud, p, sz);
  return 0;
}
static int bn_tostring(lua_State* L)
{
  bn::Number* n = bn_get(L, 1);
  int base = luaL_optinteger(L, 2, 10);
  int precision = luaL_optinteger(L, 3, -14);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  bn::tostring(n, luaStringBuilder, &b, base, precision);
  luaL_pushresult(&b);
  return 1;
}
static int bn_add(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  bn::add(bn_create(L), a, b);
  return 1;
}
static int bn_sub(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  bn::sub(bn_create(L), a, b);
  return 1;
}
static int bn_mul(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  bn::mul(bn_create(L), a, b);
  return 1;
}
static int bn_divmod(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  bn::Number* q = bn_create(L);
  bn::Number* r = bn_create(L);
  bn::divmod(q, r, a, b);
  return 2;
}
static int bn_div(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  bn::div(bn_create(L), a, b);
  return 1;
}
static int bn_mod(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  bn::mod(bn_create(L), a, b);
  return 1;
}
class BnPow : public ilua::SlowOperation
{
  bn::Number* m_a;
  bn::Number* m_b;
public:
  BnPow(bn::Number* a, bn::Number* b) : m_a(a), m_b(b) {}
  void run()
  {
    bn::Number temp;
    bn::pow(&temp, m_a, m_b);
    lua_State* L = output_begin();
    bn::Number* r = bn_create(L);
    memcpy(r, &temp, sizeof temp);
    temp.m = NULL;
    output_end();
  }
};

static int bn_pow(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  return (new(L) BnPow(a, b))->start(L);
}
static int bn_shift(lua_State* L)
{
  int shift = luaL_checkinteger(L, 2);
  bn::Number* a = bn_get(L, 1);
  bn::shift(bn_create(L), a, shift);
  return 1;
}
static int bn_sqrt(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::sqrt(bn_create(L), a);
  return 1;
}

static int bn_bits(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  lua_pushinteger(L, bn::bits(a));
  lua_pushinteger(L, bn::negbits(a));
  return 2;
}
static int bn_bit(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  int n = luaL_checkinteger(L, 2);
  lua_pushinteger(L, bn::getbit(a, n));
  return 1;
}

static int bn_clear(lua_State* L)
{
  bn::Number* a = ilua::checkstruct<bn::Number>(L, 1);
  bn::clear(a);
  return 0;
}

static int bn_floor(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::floor(bn_create(L), a);
  return 1;
}
static int bn_ceil(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::ceil(bn_create(L), a);
  return 1;
}
static int bn_round(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::round(bn_create(L), a);
  return 1;
}

static int bn_neg(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::neg(bn_create(L), a);
  return 1;
}
static int bn_abs(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::abs(bn_create(L), a);
  return 1;
}

static int bn_isneg(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  lua_pushboolean(L, bn::isneg(a));
  return 1;
}
static int bn_iszero(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  lua_pushboolean(L, bn::iszero(a));
  return 1;
}
static int bn_isone(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  lua_pushboolean(L, bn::isone(a));
  return 1;
}
static int bn_isint(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  lua_pushboolean(L, bn::isint(a));
  return 1;
}
static int bn_isodd(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  lua_pushboolean(L, bn::isodd(a));
  return 1;
}

static int bn_compare(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  int res = bn::compare(a, b);
  if (res != bn::Cmp::Unordered)
    lua_pushinteger(L, res);
  else
    lua_pushnil(L);
  return 1;
}
static int bn_equal(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  lua_pushboolean(L, bn::compare(a, b) == bn::Cmp::Equal);
  return 1;
}
static int bn_less(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  lua_pushboolean(L, bn::compare(a, b) == bn::Cmp::Less);
  return 1;
}
static int bn_lessequal(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  int cmp = bn::compare(a, b);
  lua_pushboolean(L, cmp == bn::Cmp::Less || cmp == bn::Cmp::Equal);
  return 1;
}

static int bn_gcd(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  bn::gcd(bn_create(L), a, b);
  return 1;
}
static int bn_powmod(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  bn::Number* m = bn_get(L, 3);
  bn::powmod(bn_create(L), a, b, m);
  return 1;
}
static int bn_invmod(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  bn::invmod(bn_create(L), a, b);
  return 1;
}

static int bn_band(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  bn::band(bn_create(L), a, b);
  return 1;
}
static int bn_bor(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  bn::bor(bn_create(L), a, b);
  return 1;
}
static int bn_bxor(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  bn::Number* b = bn_get(L, 2);
  bn::bxor(bn_create(L), a, b);
  return 1;
}

static int bn_precision(lua_State* L)
{
  int max_exponent = luaL_optinteger(L, 1, bn::max_exponent);
  int max_length = (lua_isnil(L, 2) ? 0x7FFFFFFF : luaL_optinteger(L, 2, bn::max_length));
  lua_pushinteger(L, bn::max_exponent);
  if (bn::max_length == 0x7FFFFFFF)
    lua_pushnil(L);
  else
    lua_pushinteger(L, bn::max_length);
  bn::max_exponent = max_exponent;
  bn::max_length = max_length;
  return 2;
}

static int bn_random(lua_State* L)
{
  int index = 1;
  RandomState* rand = random_check(L, &index);
  int bits = 0;
  if (lua_type(L, index) == LUA_TNUMBER)
    bits = lua_tointeger(L, index++);
  random_optextra(rand, L, index);
  bn::random(rand, bn_create(L), bits);
  return 1;
}
static int bn_randrange(lua_State* L)
{
  int index = 1;
  RandomState* rand = random_check(L, &index);
  bn::Number* range = bn_get(L, index++);
  random_optextra(rand, L, index);
  bn::randrange(rand, bn_create(L), range);
  return 1;
}

class BnIsPrime : public ilua::SlowOperation
{
  bn::Number* num;
  int nchecks;
public:
  BnIsPrime(bn::Number* a, int checks) : num(a), nchecks(checks) {}
  void run()
  {
    int isprime = bn::isprime(num, nchecks);
    lua_State* L = output_begin();
    lua_pushboolean(L, isprime);
    output_end();
  }
};

static int bn_isprime(lua_State* L)
{
  bn::Number* a = bn_get(L, 1);
  int checks = luaL_optinteger(L, 2, 0);
  return (new(L) BnIsPrime(a, checks))->start(L);
}

class BnRandPrime : public ilua::SlowOperation
{
  RandomState* m_rand;
  int m_bits;
public:
  BnRandPrime(RandomState* rand, int bits)
    : m_rand(rand), m_bits(bits)
  {}
  void run()
  {
    bn::Number temp;
    bn::randprime(m_rand, &temp, m_bits);
    lua_State* L = output_begin();
    bn::Number* r = bn_create(L);
    memcpy(r, &temp, sizeof temp);
    temp.m = NULL;
    output_end();
  }
};

static int bn_randprime(lua_State* L)
{
  int index = 1;
  RandomState* rand = random_check(L, &index);
  int bits = 32;
  if (lua_type(L, index) == LUA_TNUMBER)
    bits = lua_tointeger(L, index++);
  random_optextra(rand, L, index);
  return (new(L) BnRandPrime(rand, bits))->start(L);
}

static void bindfuncs(lua_State* L)
{
  ilua::bindmethod(L, "tonumber", bn_tonumber);
  ilua::bindmethod(L, "tostring", bn_tostring);
  ilua::bindmethod(L, "isneg", bn_isneg);
  ilua::bindmethod(L, "iszero", bn_iszero);
  ilua::bindmethod(L, "isone", bn_isone);
  ilua::bindmethod(L, "isint", bn_isint);
  ilua::bindmethod(L, "isodd", bn_isodd);
  ilua::bindmethod(L, "isprime", bn_isprime);
  ilua::bindmethod(L, "clear", bn_clear);
  ilua::bindmethod(L, "bits", bn_bits);
  ilua::bindmethod(L, "bit", bn_bit);
}

void bignum_bind(lua_State* L)
{
  ilua::newtype<bn::Number>(L, "bn.number");
  bindfuncs(L);
  lua_pop(L, 1);
  ilua::bindmethod(L, "__add", bn_add);
  ilua::bindmethod(L, "__sub", bn_sub);
  ilua::bindmethod(L, "__mul", bn_mul);
  ilua::bindmethod(L, "__div", bn_div);
  ilua::bindmethod(L, "__mod", bn_mod);
  ilua::bindmethod(L, "__pow", bn_pow);
  ilua::bindmethod(L, "__unm", bn_neg);
  ilua::bindmethod(L, "__eq", bn_equal);
  ilua::bindmethod(L, "__lt", bn_less);
  ilua::bindmethod(L, "__le", bn_lessequal);
  ilua::bindmethod(L, "__tostring", bn_tostring);
  lua_pop(L, 1);

  ilua::openlib(L, "bn");
  ilua::bindmethod(L, "number", bn_number);
  ilua::bindmethod(L, "add", bn_add);
  ilua::bindmethod(L, "sub", bn_sub);
  ilua::bindmethod(L, "mul", bn_mul);
  ilua::bindmethod(L, "divmod", bn_divmod);
  ilua::bindmethod(L, "div", bn_div);
  ilua::bindmethod(L, "mod", bn_mod);
  ilua::bindmethod(L, "pow", bn_pow);
  ilua::bindmethod(L, "shift", bn_shift);
  ilua::bindmethod(L, "sqrt", bn_sqrt);

  ilua::bindmethod(L, "floor", bn_floor);
  ilua::bindmethod(L, "ceil", bn_ceil);
  ilua::bindmethod(L, "round", bn_round);
  ilua::bindmethod(L, "neg", bn_neg);
  ilua::bindmethod(L, "abs", bn_abs);

  ilua::bindmethod(L, "compare", bn_compare);

  ilua::bindmethod(L, "gcd", bn_gcd);
  ilua::bindmethod(L, "powmod", bn_powmod);
  ilua::bindmethod(L, "invmod", bn_invmod);

  ilua::bindmethod(L, "band", bn_band);
  ilua::bindmethod(L, "bor", bn_bor);
  ilua::bindmethod(L, "bxor", bn_bxor);

  ilua::bindmethod(L, "precision", bn_precision);

  ilua::bindmethod(L, "random", bn_random);
  ilua::bindmethod(L, "randrange", bn_randrange);
  ilua::bindmethod(L, "randprime", bn_randprime);

  bindfuncs(L);
  lua_pop(L, 1);
}
