#include "bnimpl.h"
#include "rand.h"
#include "bnprimes.h"
#include <windows.h>

namespace bn
{

static void randbits(RandomState* rand, digit* r, int bits, int setbits)
{
  int len = (bits + digit_size - 1) / digit_size;
  int bit = ((bits + digit_size - 1) % digit_size);
  random_get(rand, r, len * sizeof(digit));
  if (bit != digit_size - 1)
    r[len - 1] &= ~(max_digit << (bit + 1));
  if (setbits)
  {
    r[0] |= 1;
    if (bit == 0)
    {
      r[len - 1] |= 1;
      if (len > 1) r[len - 2] |= high_bit;
    }
    else
      r[len - 1] |= (3 << (bit - 1));
  }
}

Number* random(RandomState* rand, Number* r, int bits)
{
  if (bits == 0)
  {
    if (max_exponent == 0) return zero(r);
    random_get(rand, resize(r, max_exponent), max_exponent * sizeof(digit));
    r->sign = 1; r->expo = -max_exponent;
    return trim(r);
  }
  else
  {
    randbits(rand, resize(r, (bits + digit_size - 1) / digit_size), bits, 0);
    r->sign = 1; r->expo = 0;
    return trim(r);
  }
}

Number* randrange(RandomState* rand, Number* r, Number const* range)
{
  if (!(range->sign & 1) || range->len == 0) return zero(r);
  assert(range->d[range->len - 1] != 0);

  int size = range->len + range->expo + max_exponent;
  if (size <= 0) return zero(r);

  int bits = digit_size;
  while ((range->d[range->len - 1] & (1 << (bits - 1))) == 0)
    bits--;
  bits += (size - 1) * digit_size;
  int count = bits + 100;

  digit* d = new digit[size];

  digit* ca = d;
  digit const* cb = range->d;
  int cn = range->len;
  if (range->expo < -max_exponent)
  {
    cb += -range->expo - max_exponent;
    cn = size;
  }
  else
    ca += range->expo + max_exponent;
  do
  {
    if (--count == 0)
    {
      delete[] d;
      return zero(r);
    }
    randbits(rand, d, bits, 0);
  } while (r_compare(ca, cb, cn) >= 0);

  return fromptr(r, d, size, range->sign, -max_exponent);
}

static int witness(MontCtx* ctx, int k, digit* p, int bits)
{
  digit* w = ctx->buf[0];
  mont_rawpow(ctx, w, w, p, ctx->buf[2], bits);
  if (r_iszero(w, ctx->size)) return 1;
  if (r_compare(w, ctx->buf[mont_r1], ctx->size) == 0) return 1;
  if (r_compare(w, ctx->buf[1], ctx->size) == 0) return 1;
  for (int i = 1; i < k; i++)
  {
    mont_mul(ctx, w, w, w);

    if (r_compare(w, ctx->buf[mont_r1], ctx->size) == 0) return 0;
    if (r_compare(w, ctx->buf[1], ctx->size) == 0) return 1;
  }
  return 0;
}

static int num_checks(int bits)
{
  if (bits < 150) return 27;
  if (bits < 200) return 18;
  if (bits < 250) return 15;
  if (bits < 300) return 12;
  if (bits < 350) return 9;
  if (bits < 400) return 8;
  if (bits < 450) return 7;
  if (bits < 550) return 6;
  if (bits < 650) return 5;
  if (bits < 850) return 4;
  if (bits < 1300) return 3;
  return 2;
}

static int r_isprime(MontCtx* ctx, RandomState* rand, digit const* a, int len, int checks)
{
  if (len == 1)
  {
    if (a[0] == 1) return 0;
    for (digit x = primes[num_primes - 1]; x < 65535; x += 2)
    {
      if (a[0] == x) return 1;
      if ((a[0] % x) == 0) return 0;
    }
    return 1;
  }

  mont_init(ctx, a, len, 4);

  int bits = digit_size;
  while ((a[len - 1] & (1 << (bits - 1))) == 0)
    bits--;
  bits += digit_size * (len - 1);

  digit* a1 = ctx->buf[3];
  int a1len = len;
  r_copy(a1, a, len);
  r_wsub(a1, len, 1);

  int k = 0;
  while (a1len && a1[0] == 0)
    a1++, a1len--, k += digit_size;

  int shift = 0;
  while ((a1[0] & (1 << shift)) == 0) shift++;
  r_shr(a1, a1, a1len, shift);
  k += shift;
  while (a1len && a1[a1len - 1] == 0) a1len--;
  if (a1len == 0) return 0;

  int a1bits = digit_size;
  while ((a1[a1len - 1] & (1 << (a1bits - 1))) == 0)
    a1bits--;
  a1bits += digit_size * (a1len - 1);

  r_copy(ctx->buf[1], ctx->buf[mont_n], ctx->size);
  r_sub(ctx->buf[1], ctx->size, ctx->buf[mont_r1], ctx->size);

  for (int i = 0; i < checks; i++)
  {
    randbits(rand, ctx->buf[0], bits, 0); ctx->buf[0][len] = 0;
    if (!witness(ctx, k, a1, a1bits)) return 0;
  }

  return 1;
}
int isprime(Number const* a, int checks)
{
  if (!isint(a) || a->len == 0 || a->expo != 0) return 0;

  for (int i = 0; i < num_primes; i++)
  {
    if (a->len == 1 && a->d[0] == primes[i]) return 1;
    if (r_mod(a->d, a->len, primes[i]) == 0) return 0;
  }

  if (checks == 0) checks = num_checks(bits(a));
  MontCtx* ctx = (MontCtx*) malloc(mont_size(a->len, 4));
  uint32 time = GetTickCount();
  RandomState* rand = random_init(NULL, HashAlgorithm::SHA256, &time, sizeof time);
  int res = r_isprime(ctx, rand, a->d, a->len, checks);
  random_free(rand);
  free(ctx);
  return res;
}

static int tryprime(digit* a, int bits)
{
  int len = (bits + digit_size - 1) / digit_size;
  digit maxd = (2 << ((bits + digit_size - 1) % digit_size)) - 1;
  digit mods[num_primes];
  for (int i = 1; i < num_primes; i++)
    mods[i] = r_mod(a, len, primes[i]);

  digit delta = 0;
  while (delta < high_bit)
  {
    bool changed = false;
    for (int i = 1; i < num_primes; i++)
    {
      if ((mods[i] + delta) % primes[i] <= 1)
      {
        delta += 2;
        changed = true;
        if (delta >= high_bit) break;
      }
    }
    if (!changed)
      return (delta == 0 || r_wadd(a, len, delta) == 0 || a[len - 1] <= maxd);
  }
  return 0;
}

Number* randprime(RandomState* rand, Number* r, int bits)
{
  if (bits <= 1) return nan(r);
  if (bits <= 14)
  {
    int last = 1;
    while (last < num_primes && primes[last] < (1 << (bits + 1)))
      last++;
    int idx = 0;
    random_get(rand, &idx, 4);
    idx &= 0x7FFFFFFF;
    return fromdigit(r, primes[idx % last]);
  }
  int checks = num_checks(bits);
  int len = (bits + digit_size - 1) / digit_size;
  MontCtx* ctx = (MontCtx*) malloc(mont_size(len, 4));
  digit* d = new digit[len];
  do
  {
    randbits(rand, d, bits, 1);
  } while (!tryprime(d, bits) || !r_isprime(ctx, rand, d, len, checks));
  free(ctx);
  return fromptr(r, d, len, 1, 0);
}

}
