#include "bnimpl.h"

namespace bn
{

Number* gcd(Number* r, Number const* a, Number const* b)
{
  if (!(a->sign & 1) || !(b->sign & 1)) return nan(r);
  int size = _max(a->len, b->len);
  digit* da = new digit[size];
  digit* db = new digit[size];
  r_copy(da, a->d, a->len); r_zero(da + a->len, size - a->len);
  r_copy(db, b->d, b->len); r_zero(db + b->len, size - b->len);
  if (r_compare(da, db, size) < 0) _swap(da, db);
  int shift = 0;
  while (!r_iszero(db, size))
  {
    if (db[0] & 1)
    {
      if (da[0] & 1)
        r_sub(da, size, db, size);
      int n = 0;
      while (n < digit_size && (da[0] & (1 << n)) == 0)
        n++;
      r_shr(da, da, size, n);
      if (r_compare(da, db, size) < 0) _swap(da, db);
    }
    else
    {
      if (da[0] & 1)
      {
        int n = 0;
        while (n < digit_size && (db[0] & (1 << n)) == 0)
          n++;
        r_shr(db, db, size, n);
      }
      else
      {
        int an = 0, bn = 0;
        while (an < digit_size - 1 && (da[0] & (1 << an)) == 0) an++;
        while (bn < digit_size - 1 && (db[0] & (1 << bn)) == 0) bn++;
        shift += _min(an, bn);
        r_shr(da, da, size, an);
        r_shr(db, db, size, bn);
        if (r_compare(da, db, size) < 0) _swap(da, db);
      }
    }
  }
  r_shl(da, da, size, shift % digit_size);
  delete[] db;
  return fromptr(r, da, size, 1, _min(a->expo, b->expo) + (shift / digit_size));
}

size_t mont_size(int size, int count)
{
  count += mont_extra;
  return sizeof(MontCtx) + sizeof(digit*) * count + sizeof(digit) * (size + 1) * count;
}
int mont_init(MontCtx* ctx, digit const* m, int len, int count)
{
  if (len == 0 || (m[0] & 1) == 0) return 0;
  int size = len + 1;
  int excount = count + mont_extra;

  memset(ctx, 0, mont_size(len, count));
  ctx->size = size;
  ctx->count = count;
  ctx->buf = (digit**) (ctx + 1);
  digit* base = (digit*) (ctx->buf + excount);
  for (int i = 0; i < excount; i++)
    ctx->buf[i] = base, base += size;
  ctx->buf += mont_extra;

  r_copy(ctx->buf[mont_n], m, len);

  int sign = -1;
  digit X = 1;
  digit Y = 1;
  digit A = -m[0];
  digit B = m[0];
  while (B)
  {
    digit M = A / B; A %= B;
    _swap(A, B);
    Y += X * M;
    _swap(X, Y);
    sign = -sign;
  }
  if (sign == 1) Y = 0 - Y;
  ctx->inv = Y;

  ctx->buf[mont_r1][size] = 1;
  r_divmod(ctx->buf[mont_r1], size + 1, m, len);
  r_divmod(ctx->buf[mont_r2], size + len, m, len);

  return 1;
}
int mont_load(MontCtx* ctx, digit* r, Number const* a)
{
  if (!isint(a)) return 0;
  if (a->len + a->expo < ctx->size && a->sign > 0)
  {
    digit* T = ctx->buf[mont_t0];
    digit* N = ctx->buf[mont_n];
    digit* K = ctx->buf[mont_r2];
    r_zero(T, ctx->size * 2);
    for (int i = 0; i < a->len; i++)
    {
      digit q = (T[0] + a->d[i] * K[0]) * ctx->inv;
      r_amul(T, ctx->size + 1, K, ctx->size, a->d[i]);
      r_amul(T, ctx->size + 1, N, ctx->size, q);
      T++;
    }
    for (int i = a->expo + a->len; i < ctx->size; i++)
    {
      digit q = T[0] * ctx->inv;
      r_amul(T, ctx->size + 1, N, ctx->size, q);
      T++;
    }
    r_copy(r, T, ctx->size);
  }
  else
  {
    int total = a->len + a->expo + ctx->size;
    digit* t = new digit[total];
    r_zero(t, ctx->size + a->expo);
    r_copy(t + ctx->size + a->expo, a->d, a->len);
    r_divmod(t, total, ctx->buf[mont_n], ctx->size);
    if (a->sign > 0 || r_iszero(t, ctx->size))
      r_copy(r, t, ctx->size);
    else
    {
      r_copy(r, ctx->buf[mont_n], ctx->size);
      r_sub(r, ctx->size, t, ctx->size);
    }
    delete[] t;
  }
  return 1;
}
void mont_mul(MontCtx* ctx, digit* r, digit const* a, digit const* b)
{
  digit* T = ctx->buf[mont_t0];
  digit* N = ctx->buf[mont_n];
  r_zero(T, ctx->size * 2);
  for (int i = 0; i < ctx->size; i++)
  {
    digit q = (T[0] + a[i] * b[0]) * ctx->inv;
    r_amul(T, ctx->size + 1, b, ctx->size, a[i]);
    r_amul(T, ctx->size + 1, N, ctx->size, q);
    T++;
  }
  r_copy(r, T, ctx->size);
}
void mont_pow(MontCtx* ctx, digit* r, digit const* a, Number const* b, digit* t)
{
  r_copy(t, a, ctx->size);
  r_zero(r, ctx->size);
  r[0] = 1;
  int count = bits(b);
  for (int bit = 0; bit < count; bit++)
  {
    if (getbit(b, bit))
      mont_mul(ctx, r, r, t);
    if (bit < count - 1)
      mont_mul(ctx, t, t, t);
  }
  if (r_compare(r, ctx->buf[mont_n], ctx->size) >= 0)
    r_sub(r, ctx->size, ctx->buf[mont_n], ctx->size);
}
void mont_rawpow(MontCtx* ctx, digit* r, digit const* a, digit const* b, digit* t, int bits)
{
  r_copy(t, a, ctx->size);
  r_copy(r, ctx->buf[mont_r1], ctx->size);
  for (int bit = 0; bit < bits; bit++)
  {
    if (b[bit / digit_size] & (1 << (bit % digit_size)))
      mont_mul(ctx, r, r, t);
    if (bit < bits - 1)
      mont_mul(ctx, t, t, t);
  }
  if (r_compare(r, ctx->buf[mont_n], ctx->size) >= 0)
    r_sub(r, ctx->size, ctx->buf[mont_n], ctx->size);
}
Number* mont_unload(MontCtx* ctx, Number* r, digit const* a, int denorm)
{
  if (denorm)
  {}
  digit* d = resize(r, ctx->size);
  r_copy(d, a, ctx->size);
  r->expo = 0;
  r->sign = 1;
  return trim(r);
}

static int r_invmod(digit const* n, digit** ab, digit** xy, digit* m, int size, int sign)
{
  int cur = 1;
  r_copy(ab[1], n, size);
  r_zero(xy[0], size * 2);
  r_zero(xy[1], size * 2);
  xy[0][0] = 1;
  while (!r_divmod(ab[cur], size, ab[1 - cur], size, m, size))
  {
    r_addmul(xy[cur], size * 2, m, size, xy[1 - cur], size);
    r_divmod(xy[cur], size * 2, n, size);
    cur = 1 - cur;
  }
  if (ab[cur][0] != 1 || !r_iszero(ab[cur] + 1, size - 1))
    return -1;
  if (cur) sign = -sign;
  if (sign < 0)
  {
    r_copy(xy[1 - cur], n, size);
    r_sub(xy[1 - cur], size, xy[cur], size);
    cur = 1 - cur;
  }
  return cur;
}
Number* invmod(Number* r, Number const* a, Number const* b)
{
  if (!isint(a) || !isint(b) || b->len == 0) return nan(r);
  int size = b->len + b->expo;
  digit* buf[6];
  buf[0] = new digit[size * 2]; // x
  buf[1] = new digit[size * 2]; // y
  buf[2] = new digit[size]; // n
  buf[3] = new digit[size]; // m
  buf[4] = new digit[_max(a->len + a->expo, size)]; // a
  buf[5] = new digit[size]; // b
  r_zero(buf[2], b->expo);
  r_copy(buf[2] + b->expo, b->d, b->len);
  r_zero(buf[4], a->expo);
  r_copy(buf[4] + a->expo, a->d, a->len);
  if (a->len + a->expo < size)
    r_zero(buf[4] + a->len + a->expo, size - a->len + a->expo);
  else
    r_divmod(buf[4], a->len + a->expo, buf[2], size);
  int cur = r_invmod(buf[2], buf + 4, buf, buf[3], size, a->sign);
  if (cur < 0)
    nan(r), cur = 0, delete[] buf[cur];
  else
    fromptr(r, buf[cur], size, 1, 0);
  delete[] buf[1 - cur];
  delete[] buf[2]; delete[] buf[3]; delete[] buf[4]; delete[] buf[5];
  return r;
}
Number* powmod(Number* r, Number const* a, Number const* b, Number const* m)
{
  if (!isint(a) || !isint(b) || !isint(m) || m->len == 0) return nan(r);

  if (isodd(m))
  {
    MontCtx* ctx = (MontCtx*) malloc(mont_size(m->len, 2));
    mont_init(ctx, m->d, m->len, 2);
    mont_load(ctx, ctx->buf[0], a);
    mont_pow(ctx, ctx->buf[0], ctx->buf[0], b, ctx->buf[1]);
    if (b->sign > 0)
      mont_unload(ctx, r, ctx->buf[0], 0);
    else
    {
      digit* xy[2];
      xy[0] = new digit[m->len * 2];
      xy[1] = new digit[m->len * 2];
      int cur = r_invmod(ctx->buf[mont_n], ctx->buf, xy, ctx->buf[mont_r1], m->len, 1);
      if (cur < 0)
        nan(r), cur = 0, delete[] xy[cur];
      else
        fromptr(r, xy[cur], m->len, 1, 0);
      delete[] xy[1 - cur];
    }
    free(ctx);
    return r;
  }
  else
  {
    int size = m->len + m->expo;
    digit* md = new digit[size];
    digit* ad[2];
    ad[0] = new digit[_max(a->len + a->expo, size * 2)];
    ad[1] = new digit[size * 2];
    digit* bd[2];
    bd[0] = new digit[size * 2];
    bd[1] = new digit[size * 2];
    int ca = 0;
    int cb = 0;
    r_zero(md, m->expo);
    r_copy(md + m->expo, m->d, m->len);
    r_zero(ad[0], a->expo);
    r_copy(ad[0] + a->expo, a->d, a->len);
    if (a->len + a->expo < size)
      r_zero(ad[0] + a->len + a->expo, size - a->len + a->expo);
    else
      r_divmod(ad[0], a->len + a->expo, md, size);
    if (b->sign < 0)
    {
      digit* t = new digit[size];
      int cur = r_invmod(md, ad, bd, t, size, a->sign);
      delete[] t;
      if (cur < 0)
      {
        delete[] md;
        delete[] ad[0]; delete[] ad[1];
        delete[] bd[0]; delete[] bd[1];
        return nan(r);
      }
      _swap(bd[cur], ad[0]);
    }
    else if (a->sign < 0 && a->len > 0)
    {
      r_copy(ad[1 - ca], md, size);
      r_sub(ad[1 - ca], size, ad[ca], size);
      ca = 1 - ca;
    }
    r_zero(bd[0], size);
    bd[0][0] = 1;
    int count = bits(b);
    for (int i = 0; i < count; i++)
    {
      if (getbit(b, i))
      {
        r_zero(bd[1 - cb], size * 2);
        r_addmul(bd[1 - cb], size * 2, bd[cb], size, ad[ca], size);
        cb = 1 - cb;
        r_divmod(bd[cb], size * 2, md, size);
      }
      if (i < count - 1)
      {
        r_zero(ad[1 - ca], size * 2);
        r_addmul(ad[1 - ca], size * 2, ad[ca], size, ad[ca], size);
        ca = 1 - ca;
        r_divmod(ad[ca], size * 2, md, size);
      }
    }
    delete[] md;
    delete[] ad[0];
    delete[] ad[1];
    delete[] bd[1 - cb];
    return fromptr(r, bd[cb], size, 1, 0);
  }
}

}
