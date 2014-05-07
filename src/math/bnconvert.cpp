#include "bnimpl.h"
#include <ctype.h>

namespace bn
{

Number* fromdouble(Number* r, double num)
{
  uint64 rep = *(uint64*) &num;

  resize(r, (64 / digit_size) + 1);
  r->sign = ((rep & DBL_NEG) ? -1 : 1);

  int expo = ((rep >> 52) & 0x7FF) - 1023;
  rep &= (DBL_MAX - 1);

  if (expo == -1023)
    expo = -1022;
  else if (expo == 1024)
  {
    if (rep == 0)
      r->sign *= 2;
    else
      r->sign = Sign::NaN;
  }
  else
    rep |= DBL_MAX;
  int shift = ((expo - 52) & (digit_size - 1));
  r->expo = ((expo - 52) >> log_digit_size);
  r->d[r->len - 1] = digit(rep >> (64 - shift));
  rep <<= shift;
  memcpy(r->d, &rep, sizeof rep);

  return trim(r);
}

double todouble(Number const* a)
{
  uint64 rep = 0;

  if (a->sign == Sign::NaN)
    rep = 0x7FFFFFFFFFFFFFFFULL;
  else if (a->sign == Sign::PosInfinity)
    rep = 0x7FF0000000000000ULL;
  else if (a->sign == Sign::NegInfinity)
    rep = 0xFFF0000000000000ULL;
  else if (a->len == 0)
    rep = (a->sign < 0 ? DBL_NEG : 0);
  else
  {
    digit* src = a->d + a->len - 1;
    digit* dst = ((digit*) &rep) + (64 / digit_size) - 1;
    for (int i = 0; i < (64 / digit_size) && i < a->len; i++)
      *dst-- = *src--;
    assert(rep != 0);
    int expo = (a->expo + a->len) * digit_size - 12;
    while (rep >= (DBL_MAX << 1))
      rep >>= 1, expo++;
    int shift = 0;
    while ((rep & DBL_MAX) == 0)
      rep <<= 1, expo--, shift++;
    if (a->len > (64 / digit_size) && shift)
      rep |= (a->d[a->len - (64 / digit_size)] >> (digit_size - shift));
    if (a->len > (64 / digit_size) && (a->d[a->len - (64 / digit_size)] & (1 << (digit_size - 1 - shift))))
      rep++;
    if (rep >= (DBL_MAX << 1))
      rep >>= 1, expo++;
    if (expo < -1074)
      expo = 0, rep = 0;
    else if (expo < -1022)
      rep >>= -(expo + 1022), expo = 0;
    else if (expo >= 1024)
      rep = 0, expo = 0x7FF;
    else
      rep &= (DBL_MAX - 1), expo += 1023;
    rep |= (uint64(expo) << 52);
    if (a->sign < 0)
      rep |= DBL_NEG;
  }

  return *(double*) &rep;
}

int tostring(Number const* a, StringBuilder builder, void* ud, int base, int precision)
{
  static char qdigits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  if (a->sign == Sign::NaN)
    return builder("1.#QNAN", 7, ud);
  if (a->sign == Sign::PosInfinity)
    return builder("1.#INF", 6, ud);
  if (a->sign == Sign::NegInfinity)
    return builder("-1.#INF", 7, ud);
  if (a->len == 0)
    return base == 256 ? 0 : builder("0", 1, ud);

  int res = 0;
  if (base == 10)
  {
    int aprec = (precision > 0 ? precision : -precision);
    int above = _max(a->len + a->expo, 0);
    int below = _min(aprec / int(2 * sizeof(digit)) + 2, _max(0, -a->expo));
    if (above == 0 && below == 0)
      return builder("0", 1, ud);

    int size = above * 3 * sizeof(digit) + aprec + 10;
    char* str = new char[size];
    char* cur = str + size;
    *--cur = 0;
    char* end = cur;

    digit* tmp = new digit[_max(above, below)];

    digit uc = 0;
    if (aprec)
    {
      if (below == 0)
      {
        for (int i = 0; i < precision; i++)
          *--cur = '0';
      }
      else
      {
        r_zero(tmp, below);
        tmp[below - 1] = high_bit;
        for (int i = 0; i < aprec; i++)
          r_div(tmp, tmp, below, 10);

        int alen = a->len + a->expo + below;
        if (alen > 0)
          uc = r_add(tmp, below, a->d - a->expo - below, alen);
        cur -= aprec;
        char* ptr = cur;
        for (int i = 0; i < aprec; i++)
        {
          digit c = r_mul(tmp, below, tmp, below, 10);
          *ptr++ = char('0' + c);
        }
        if (precision < 0)
        {
          while (ptr > cur && ptr[-1] == '0')
            *--ptr = 0;
        }
      }
      if (*cur)
        *--cur = '.';
    }
    if (above)
    {
      if (a->expo >= 0)
      {
        r_zero(tmp, a->expo);
        r_copy(tmp + a->expo, a->d, a->len);
      }
      else
        r_copy(tmp, a->d - a->expo, a->len + a->expo);
      if (uc) r_wadd(tmp, above, uc);
      while (true)
      {
        digit c = r_div(tmp, tmp, above, 10);
        *--cur = char('0' + c);
        if (r_iszero(tmp, above))
          break;
      }
    }
    else
      *--cur = '0';
    delete[] tmp;
    if (a->sign < 0)
      *--cur = '-';
    int res = builder(cur, end - cur, ud);
    delete[] str;
    return res;
  }
  else if (base == 16)
  {
    if (a->sign < 0 && (res = builder("-", 1, ud)))
      return res;
    if (res = builder("0x", 2, ud)) return res;

    if (a->len + a->expo > 0)
    {
      for (int i = a->len - 1; i >= 0 && i >= -a->expo; i--)
        for (int j = 2 * sizeof(digit) - 1; j >= 0; j--)
          if (i < a->len - 1 || a->d[i] >= digit(1 << (j * 4)))
            if (res = builder(qdigits + ((a->d[i] >> (j * 4)) & 0xF), 1, ud))
              return res;
      for (int i = 0; i < a->expo; i++)
        if (res = builder("00000000", sizeof(digit) * 2, ud))
          return res;
    }
    else if (res = builder("0", 1, ud))
      return res;
    if (a->expo < 0)
    {
      if (res = builder(".", 1, ud)) return res;
      for (int i = 0; i < -a->expo - a->len; i++)
        if (res = builder("00000000", sizeof(digit) * 2, ud))
          return res;
      for (int i = _min(-a->expo, a->len) - 1; i >= 0; i--)
        for (int j = 2 * sizeof(digit) - 1; j >= 0; j--)
          if (i > 0 || (a->d[i] & ((1 << (j * 4 + 4)) - 1)))
            if (res = builder(qdigits + ((a->d[i] >> (j * 4)) & 0xF), 1, ud))
              return res;
    }

    return 0;
  }
  else if (base >= 2 && base <= 36)
  {
    if (a->len + a->expo > 0)
    {
      int size = (a->len + a->expo) * digit_size + 5;
      char* str = new char[size];
      char* cur = str + size;
      *--cur = 0;
      char* end = cur;
      digit* tmp = new digit[a->len + a->expo];
      if (a->expo >= 0)
      {
        r_zero(tmp, a->expo);
        r_copy(tmp + a->expo, a->d, a->len);
      }
      else
        r_copy(tmp, a->d - a->expo, a->len + a->expo);
      while (true)
      {
        digit c = r_div(tmp, tmp, a->len + a->expo, base);
        *--cur = qdigits[c];
        if (r_iszero(tmp, a->len + a->expo))
          break;
      }
      delete[] tmp;
      if (a->sign < 0)
        *--cur = '-';
      int res = builder(cur, end - cur, ud);
      delete[] str;
      return res;
    }
    else
      return builder("0", 1, ud);
  }
  else if (base == 256)
  {
    if (a->len + a->expo > 0)
    {
      int top = 0;
      while ((a->d[a->len - 1] & (digit(0xFF) << (digit_size + 8 * (top - 1)))) == 0)
        top--;
      for (int i = 0; i < a->expo; i++)
        if (res = builder("0000", sizeof(digit), ud))
          return res;
      return builder((char*) (a->d - _min(a->expo, 0)), sizeof(digit) * (a->len + _min(a->expo, 0)) + top, ud);
    }
    return 0;
  }
  return 1;
}

static inline digit bvalue(unsigned char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'z') return c - 'a' + 10;
  if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
  return max_digit;
}

Number* fromstring(Number* r, char const* str, int length, int base)
{
  if (length < 0) length = strlen(str);
  char const* end = str + length;
  if (base == 256)
  {
    digit* d = resize(r, (length + sizeof(digit) - 1) / sizeof(digit));
    if (length % sizeof(digit)) d[r->len - 1] = 0;
    memcpy(d, str, length);
    r->expo = 0;
    r->sign = 1;
    return trim(r);
  }
  else if (base >= 2 && base <= 36)
  {
    for (int i = 0; i < length; i++)
      if (bvalue(str[i]) >= digit(base))
        return NULL;
    int count = 0;
    digit cx = max_digit;
    while (cx) cx /= base, count++;
    digit* d = resize(r, (length + count - 2) / (count - 1));
    r_zero(d, r->len);
    r->sign = 1;
    r->expo = 0;
    for (int i = 0; i < length; i++)
      r_mul(d, r->len, d, r->len, base, bvalue(str[i]));
    return trim(r);
  }
  else if (base)
    return NULL;

  while (isspace((unsigned char) *str))
    str++;
  int sign = 1;
  if (str[0] == '-')
    sign = -1, str++;
  else if (str[0] == '+')
    str++;
  if (strstr(str, "INF"))
    return infinity(r, sign);
  if (str[0] == 'x' || str[0] == 'X' || (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')))
  {
    str += (str[0] == '0' ? 2 : 1);

    char const* save = str;
    int len = 0;
    while (isxdigit((unsigned char) *str))
      str++, len++;
    if (*str == '.')
    {
      str++;
      while (isxdigit((unsigned char) *str))
        str++, len++;
    }
    if (len == 0)
      return NULL;
    int bexpo = 0;
    if (*str == 'p' || *str == 'P')
    {
      str++;
      int e = 0;
      int esign = 1;
      if (*str == '-')
        esign = -1, str++;
      else if (*str == '+')
        str++;
      if (!isdigit((unsigned char) *str))
        return NULL;
      while (isdigit((unsigned char) *str))
        e = e * 10 + (*str++) - '0';
      bexpo -= e * esign;
    }
    if (str != end)
      return NULL;

    str = save;

    const int dsize = 2 * sizeof(digit);
    digit* d = resize(r, (len + dsize - 1) / dsize + 1);
    r->sign = sign;
    r->expo = 0;
    r_zero(d, r->len);
    int count = 0;
    while (isxdigit((unsigned char) *str))
      d[r->len - 1 - (count / dsize)] |= (bvalue(*str++) << (digit_size - ((count % dsize) + 1) * 4)), count++;
    if (*str == '.')
    {
      str++;
      while (isxdigit((unsigned char) *str))
        d[r->len - 1 - (count / dsize)] |= (bvalue(*str++) << (digit_size - ((count % dsize) + 1) * 4)), count++, bexpo += 4;
    }
    bexpo += (r->len * 4 - count) * 4;

    r->expo = -(bexpo / digit_size);
    bexpo %= digit_size;
    r_shr(d, d, r->len, bexpo);

    return trim(r);
  }

  char const* save = str;
  int len = 0;
  int dexpo = 0;
  while (isdigit((unsigned char) *str))
    str++, len++;
  if (*str == '.')
  {
    str++;
    while (isdigit((unsigned char) *str))
      str++, len++, dexpo--;
  }
  if (len == 0)
    return NULL;
  if (*str == 'e' || *str == 'E')
  {
    str++;
    int e = 0;
    int esign = 1;
    if (*str == '-')
      esign = -1, str++;
    else if (*str == '+')
      str++;
    if (!isdigit((unsigned char) *str))
      return NULL;
    while (isdigit((unsigned char) *str))
      e = e * 10 + (*str++) - '0';
    dexpo += e * esign;
  }
  if (str != end)
    return NULL;

  int count = -1;
  digit cx = max_digit;
  while (cx) cx /= 10, count++;

  str = save;
  digit* d = resize(r, dexpo < 0 ? (len + count - 1) / count + max_exponent : (len + dexpo + count - 1) / count);
  r->sign = sign;
  r->expo = (dexpo < 0 ? -max_exponent : 0);
  r_zero(d, r->len);

  digit* id = d - r->expo;
  int il = r->len + r->expo;

  while (isdigit((unsigned char) *str))
    r_mul(id, il, id, il, 10, *str++ - '0');
  if (*str == '.')
  {
    str++;
    while (isdigit((unsigned char) *str))
      r_mul(id, il, id, il, 10, *str++ - '0');
  }

  while (dexpo > 0)
    r_mul(d, r->len, d, r->len, 10), dexpo--;
  while (dexpo < 0)
    r_div(d, d, r->len, 10), dexpo++;
  return trim(r);
}

}
