#include "bnimpl.h"

namespace bn
{

int max_length = 0x7FFFFFFF;
int max_exponent = 0;

digit* resize(Number* a, int len)
{
  if (a->m && a->len >= len) return a->len = len, a->d;
  delete[] a->m;
  a->len = len;
  return a->m = a->d = new digit[len ? len : 1];
}
digit* expand(Number* a, int len, int expo)
{
  if (a->m == NULL)
  {
    a->expo = expo;
    a->len = len;
    a->m = a->d = new digit[len ? len : 1];
    r_zero(a->d, len);
    return a->d;
  }
  if (a->len >= len)
  {
    if (expo < a->expo)
    {
      memmove(a->d + a->expo - expo, a->d, sizeof(digit) * (len - a->expo + expo));
      r_zero(a->d, a->expo - expo);
    }
    else if (expo > a->expo)
    {
      int alen = a->len + a->expo - expo;
      memmove(a->d, a->d + expo - a->expo, sizeof(digit) * _min(alen, len));
      if (alen < len)
        r_zero(a->d + alen, len - alen);
    }
    a->expo = expo;
    a->len = len;
    return a->d;
  }
  digit* t = new digit[len];
  r_zero(t, len);
  if (a->expo >= expo && a->expo - expo < len)
    r_copy(t + a->expo - expo, a->d, _min(a->len, len - a->expo + expo));
  else if (a->expo < expo && a->len + a->expo - expo > 0)
    r_copy(t, a->d + expo - a->expo, _min(len, a->len + a->expo - expo));
  delete[] a->m;
  a->len = len;
  a->expo = expo;
  return a->m = a->d = t;
}
void copydigits(Number* r, Number const* a)
{
  r_zero(r->d, r->len);
  if (a->expo >= r->expo && a->expo - r->expo < r->len)
    r_copy(r->d + a->expo - r->expo, a->d, _min(a->len, r->len - a->expo + r->expo));
  else if (a->expo < r->expo && a->len + a->expo - r->expo > 0)
    r_copy(r->d, a->d + r->expo - a->expo, _min(r->len, a->len + a->expo - r->expo));
}
Number* fromdigit(Number* a, digit d, int sign)
{
  resize(a, d ? 1 : 0);
  a->sign = (sign > 0 ? 1 : -1); a->expo = 0;
  a->d[0] = d;
  return a;
}
Number* zero(Number* a, int sign)
{
  resize(a, 0);
  a->sign = (sign > 0 ? 1 : -1); a->expo = 0;
  a->d[0] = 0;
  return a;
}
Number* infinity(Number* a, int sign)
{
  a->sign = (sign > 0 ? 2 : -2); a->len = 0; a->expo = 0;
  return a;
}
Number* nan(Number* a)
{
  a->sign = 0; a->expo = 0; a->len = 0;
  return a;
}

Number* trim(Number* a)
{
  while (a->len > 0 && a->d[a->len - 1] == 0)
    a->len--;
  if (a->len > max_length || -a->expo > max_exponent)
  {
    int d = _min(a->len, _max(a->len - max_length, -a->expo - max_exponent));
    a->len -= d;
    a->expo += d;
    a->d += d;
  }
  while (a->len > 0 && a->d[0] == 0)
    a->d++, a->expo++, a->len--;
  return a;
}
Number* copy(Number* r, Number const* a)
{
  if (r == a) return r;
  resize(r, a->len);
  r_copy(r->d, a->d, a->len);
  r->expo = a->expo;
  r->sign = a->sign;
  return r;
}
Number* fromptr(Number* r, digit* d, int len, int sign, int expo)
{
  if (r->d != d)
  {
    delete[] r->m;
    r->m = r->d = d;
  }
  r->len = len;
  r->sign = sign;
  r->expo = expo;
  return trim(r);
}

Number* neg(Number* r, Number const* a)
{
  copy(r, a);
  r->sign = -r->sign;
  return r;
}
Number* abs(Number* r, Number const* a)
{
  copy(r, a);
  if (r->sign < 0) r->sign = -r->sign;
  return r;
}
Number* clear(Number* a)
{
  if (a->d)
    r_zero(a->d, a->len);
  else
    resize(a, 0);
  a->len = 0;
  a->expo = 0;
  a->sign = 1;
  return a;
}
int bits(Number const* a)
{
  if (!(a->sign & 1)) return 0;
  if (a->len == 0 || a->len + a->expo <= 0) return 0;
  assert(a->d[a->len - 1] != 0);
  int bits = digit_size;
  while ((a->d[a->len - 1] & (1 << (bits - 1))) == 0)
    bits--;
  return bits + (a->len + a->expo - 1) * digit_size;
}
int negbits(Number const* a)
{
  if (!(a->sign & 1)) return 0;
  if (a->len == 0 || a->expo >= 0) return 0;
  assert(a->d[0] != 0);
  int bits = 0;
  while ((a->d[0] & (1 << bits)) == 0)
    bits++;
  return (-a->expo) * digit_size - bits;
}
int getbit(Number const* a, int bit)
{
  int d = (bit >> log_digit_size) - a->expo;
  if (d < 0 || d >= a->len)
    return 0;
  return (a->d[d] & (1 << (bit & (digit_size - 1)))) != 0;
}

}
