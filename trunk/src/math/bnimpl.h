#ifndef __MATH_BNIMPL__
#define __MATH_BNIMPL__

#include "bignum.h"
#include "base/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma warning(disable: 4146)

namespace bn
{

typedef uint64 bigdigit;
const int digit_size = (sizeof(digit) * 8);
const int log_digit_size = 5;
const digit high_bit = (digit(1) << (digit_size - 1));
const digit max_digit = digit(-1);
const bigdigit big_overflow = (bigdigit(1) << digit_size);
#define MAKEBIG(h,l)  ((bigdigit(h)<<digit_size)+bigdigit(l))

const int mont_extra = 5;
const int mont_n = -5;
const int mont_r2 = -4;
const int mont_r1 = -3;
const int mont_t0 = -2;
const int mont_t1 = -1;

#define _min(a,b)     ((a)<(b)?(a):(b))
#define _max(a,b)     ((a)>(b)?(a):(b))
#define DBL_MAX       0x0010000000000000ULL
#define DBL_NEG       0x8000000000000000ULL

template<class T>
inline void _swap(T& a, T& b)
{
  T c = a;
  a = b;
  b = c;
}

inline void r_zero(digit* dst, int len)
{
  _asm
  {
    cld
    mov ecx, dword ptr[len]
    mov edi, dword ptr[dst]
    mov eax, 0
    rep stosd
  }
}
inline void r_copy(digit* dst, digit const* src, int len)
{
  _asm
  {
    cld
    mov ecx, dword ptr[len]
    mov esi, dword ptr[src]
    mov edi, dword ptr[dst]
    rep movsd
  }
}

digit r_wadd(digit* r, int rn, digit a);
digit r_wsub(digit* r, int rn, digit a);
digit r_add(digit* r, int rn, digit const* a, int an);
digit r_add2(digit* r, int rn, digit const* a, int an);
digit r_sub(digit* r, int rn, digit const* a, int an);
digit r_mul(digit* r, int rn, digit const* a, int an, digit b, digit c = 0);
digit r_amul(digit* r, int rn, digit const* a, int an, digit b);
digit r_smul(digit* r, int rn, digit const* a, int an, digit b);
digit r_div(digit* r, digit const* a, int n, digit b);
digit r_mod(digit const* a, int n, digit b);
digit r_shl(digit* r, digit const* a, int n, int s, digit c = 0);
digit r_shr(digit* r, digit const* a, int n, int s, digit c = 0);
int r_compare(digit const* a, digit const* b, int n);

digit* resize(Number* a, int len);
digit* expand(Number* a, int len, int expo);
void copydigits(Number* r, Number const* a);

Number* trim(Number* a);
Number* fromptr(Number* r, digit* d, int len, int sign = 1, int expo = 0);
int rawcompare(Number const* a, Number const* b, int sign);

Number* rawadd(Number* r, Number const* a, Number const* b);
Number* rawsub(Number* r, Number const* a, Number const* b);

inline int r_multempsize(int n)
{
  return 5 * ((n + 1) / 2);
}
void r_addmul(digit* r, int rn, digit const* a, int an, digit const* b, int bn, digit* t = NULL);
void r_addmulx(digit* r, int rn, digit const* a, int an, digit const* b, int bn, digit* t = NULL);
int r_divmod(digit* a, int an, digit const* b, int bn, digit* q = NULL, int qn = 0);

inline int r_iszero(digit const* d, int n)
{
  for (int i = 0; i < n; i++)
    if (d[i]) return 0;
  return 1;
}

void mont_rawpow(MontCtx* ctx, digit* r, digit const* a, digit const* b, digit* t, int bits);

}

#endif // __MATH_BNIMPL__