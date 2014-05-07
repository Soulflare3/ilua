#ifndef __MATH_BIGNUM__
#define __MATH_BIGNUM__

#include "base/types.h"
struct RandomState;
#include <stdlib.h>

namespace bn
{

typedef uint32 digit;

namespace Sign
{
  enum {NegInfinity = -2,
        Minus       = -1,
        NaN         = 0,
        Plus        = 1,
        PosInfinity = 2,
  };
}
namespace Cmp
{
  enum {
    Unordered = 0x80000000,
    Less      = -1,
    Equal     = 0,
    Greater   = 1,
  };
}

struct Number
{
  int sign;
  int len;
  int expo;
  digit* d;
  digit* m;

  Number()
    : m(NULL), d(NULL)
  {}
  ~Number()
  {
    delete[] m;
  }
};

extern int max_length;
extern int max_exponent;

Number* fromdigit(Number* a, digit d, int sign = 1);
Number* zero(Number* a, int sign = 1);
Number* infinity(Number* a, int sign = 1);
Number* nan(Number* a);
Number* copy(Number* r, Number const* a);

int compare(Number const* a, Number const* b);

Number* band(Number* r, Number const* a, Number const* b);
Number* bor(Number* r, Number const* a, Number const* b);
Number* bxor(Number* r, Number const* a, Number const* b);

Number* add(Number* r, Number const* a, Number const* b);
Number* sub(Number* r, Number const* a, Number const* b);
Number* mul(Number* r, Number const* a, Number const* b);
Number* div(Number* q, Number const* a, Number const* b);
Number* mod(Number* r, Number const* a, Number const* b);
Number* divmod(Number* q, Number* r, Number const* a, Number const* b);
Number* pow(Number* r, Number const* a, Number const* b);
Number* shift(Number* r, Number const* a, int left);

Number* sqrt(Number* r, Number const* a);

int bits(Number const* a);
int negbits(Number const* a);
int getbit(Number const* a, int bit);

Number* clear(Number* a);

Number* floor(Number* r, Number const* a);
Number* ceil(Number* r, Number const* a);
Number* round(Number* r, Number const* a);

Number* neg(Number* r, Number const* a);
Number* abs(Number* r, Number const* a);

Number* fromdouble(Number* r, double num);
double todouble(Number const* a);

Number* fromstring(Number* r, char const* str, int length = -1, int base = 0);

typedef int (*StringBuilder)(char const* p, size_t sz, void* ud);
int tostring(Number const* a, StringBuilder builder, void* ud, int base = 10, int precision = 0);

inline int isneg(Number const* a)
{
  return (a->sign == -1 && a->len > 0) || a->sign == -2;
}
inline int iszero(Number const* a)
{
  return ((a->sign & 1) && a->len == 0);
}
inline int isone(Number const* a)
{
  return (a->sign == 1 && a->len == 1 && a->expo == 0 && a->d[0] == 1);
}
inline int isint(Number const* a)
{
  return ((a->sign & 1) && (a->expo >= 0 || a->len == 0));
}
inline int isodd(Number const* a)
{
  return ((a->sign & 1) && a->expo <= 0 && a->len > -a->expo && (a->d[-a->expo] & 1));
}

struct MontCtx
{
  int size;
  int count;
  digit inv;
  digit** buf;
};
size_t mont_size(int size, int count);
int mont_init(MontCtx* ctx, digit const* m, int size, int count);

int mont_load(MontCtx* ctx, digit* r, Number const* a);
Number* mont_unload(MontCtx* ctx, Number* r, digit const* a, int denorm);
void mont_mul(MontCtx* ctx, digit* r, digit const* a, digit const* b);
void mont_pow(MontCtx* ctx, digit* r, digit const* a, Number const* b, digit* t);

Number* gcd(Number* r, Number const* a, Number const* b);
Number* invmod(Number* r, Number const* a, Number const* b);
Number* powmod(Number* r, Number const* a, Number const* b, Number const* m);

Number* random(RandomState* rand, Number* r, int bits);
Number* randrange(RandomState* rand, Number* r, Number const* range);
int isprime(Number const* a, int checks = 0);
Number* randprime(RandomState* rand, Number* r, int bits);

}

#endif // __MATH_BIGNUM__
