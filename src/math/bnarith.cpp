#include "bnimpl.h"

namespace bn
{

Number* band(Number* r, Number const* a, Number const* b)
{
  if (!(a->sign & 1) || !(b->sign & 1)) return nan(r);
  if (r == b) _swap(a, b);

  int expo = _max(a->expo, b->expo);
  int len = _min(a->len + a->expo, b->len + b->expo) - expo;
  if (len <= 0) return zero(r);
  if (a == r)
    expand(r, len, expo);
  else
  {
    resize(r, len);
    r->sign = a->sign;
    r->expo = expo;
    r_copy(r->d, a->d + expo - a->expo, len);
  }

  digit* dst = r->d;
  digit* src = b->d + expo - b->expo;
  for (int i = len; i != 0; i--)
    *dst++ &= *src++;

  return trim(r);
}
Number* bor(Number* r, Number const* a, Number const* b)
{
  if (!(a->sign & 1) || !(b->sign & 1)) return nan(r);
  if (r == b) _swap(a, b);

  int expo = _min(a->expo, b->expo);
  int len = _max(a->len + a->expo, b->len + b->expo) - expo;
  if (a == r)
    expand(r, len, expo);
  else
  {
    resize(r, len);
    r->sign = a->sign;
    r->expo = expo;
    r_zero(r->d, len);
    r_copy(r->d + a->expo - expo, a->d, a->len);
  }

  digit* dst = r->d + b->expo - expo;
  digit* src = b->d;
  for (int i = b->len; i != 0; i--)
    *dst++ |= *src++;

  return trim(r);
}
Number* bxor(Number* r, Number const* a, Number const* b)
{
  if (!(a->sign & 1) || !(b->sign & 1)) return nan(r);
  if (r == b) _swap(a, b);

  int expo = _min(a->expo, b->expo);
  int len = _max(a->len + a->expo, b->len + b->expo) - expo;
  if (a == r)
    expand(r, len, expo);
  else
  {
    resize(r, len);
    r->sign = a->sign;
    r->expo = expo;
    r_zero(r->d, len);
    r_copy(r->d + a->expo - expo, a->d, a->len);
  }

  digit* dst = r->d + b->expo - expo;
  digit* src = b->d;
  for (int i = b->len; i != 0; i--)
    *dst++ ^= *src++;

  return trim(r);
}

int r_compare(digit const* a, digit const* b, int n)
{
  for (int i = n - 1; i >= 0; i--)
  {
    if (a[i] < b[i]) return -1;
    if (a[i] > b[i]) return 1;
  }
  return 0;
}

int rawcompare(Number const* a, Number const* b, int sign)
{
  if (sign && (!(a->sign & 1) || !(b->sign & 1)))
  {
    if (a->sign == 0 || b->sign == 0)
      return Cmp::Unordered;
    if (a->sign > b->sign)
      return 1;
    if (a->sign < b->sign)
      return -1;
    return 0;
  }
  if (a->len == 0 && b->len == 0)
    return 0;
  if (sign && a->sign != b->sign)
    return a->sign;
  int m = (sign ? a->sign : 1);
  if (a->expo + a->len > b->expo + b->len)
    return m;
  if (a->expo + a->len < b->expo + b->len)
    return -m;
  int ia = a->len - 1;
  int ib = b->len - 1;
  while (ia >= 0 && ib >= 0)
  {
    if (a->d[ia] > b->d[ib])
      return m;
    if (a->d[ia] < b->d[ib])
      return -m;
    ia--;
    ib--;
  }
  if (a->len > b->len)
    return m;
  if (a->len < b->len)
    return -m;
  return 0;
}
int compare(Number const* a, Number const* b)
{
  return rawcompare(a, b, 1);
}

Number* rawadd(Number* r, Number const* a, Number const* b)
{
  if (r == b)
    _swap(a, b);

  int expo = _min(a->expo, b->expo);
  int len = _max(a->len + a->expo, b->len + b->expo) + 1 - expo;
  if (a == r)
    expand(r, len, expo);
  else
  {
    resize(r, len);
    r->sign = a->sign;
    r->expo = expo;
    r_zero(r->d, len);
    r_copy(r->d + a->expo - expo, a->d, a->len);
  }

  r_add(r->d + b->expo - expo, len + expo - b->expo, b->d, b->len);

  return trim(r);
}
Number* rawsub(Number* r, Number const* a, Number const* b)
{
  int expo = _min(a->expo, b->expo);
  int len = _max(a->len + a->expo, b->len + b->expo) - expo;
  digit* d;
  if (a == r)
    d = expand(r, len, expo);
  else
  {
    if (b == r)
      d = new digit[len];
    else
      d = resize(r, len);
    r_zero(d, len);
    r_copy(d + a->expo - expo, a->d, a->len);
  }
  r_sub(d + b->expo - expo, len + expo - b->expo, b->d, b->len);
  return fromptr(r, d, len, a->sign, expo);
}

Number* add(Number* r, Number const* a, Number const* b)
{
  if (!(a->sign & 1) || !(b->sign & 1))
  {
    if (a->sign == 0 || b->sign == 0 || a->sign == -b->sign)
      return nan(r);
    if (a->sign & 1)
      return infinity(r, b->sign);
    else
      return infinity(r, a->sign);
  }
  if (a->sign == b->sign)
    return rawadd(r, a, b);
  switch (rawcompare(a, b, 0))
  {
  case -1:  return rawsub(r, b, a);
  case 1:   return rawsub(r, a, b);
  }
  return zero(r);
}
Number* sub(Number* r, Number const* a, Number const* b)
{
  if (!(a->sign & 1) || !(b->sign & 1))
  {
    if (a->sign == 0 || b->sign == 0 || a->sign == b->sign)
      return nan(r);
    if (a->sign & 1)
      return infinity(r, -b->sign);
    else
      return infinity(r, a->sign);
  }
  if (a->sign != b->sign)
    return rawadd(r, a, b);
  switch (rawcompare(a, b, 0))
  {
  case -1:
    rawsub(r, b, a);
    r->sign = -r->sign;
    return r;
  case 1:
    return rawsub(r, a, b);
  }
  return zero(r);
}

void r_addmul(digit* r, int rn, digit const* a, int an, digit const* b, int bn, digit* t)
{
  if (an > rn) an = rn;
  if (bn > rn) bn = rn;
  if (an < bn)
    _swap(an, bn), _swap(a, b);
  if (bn <= 32)
  {
    for (int i = 0; i < bn; i++)
      r_amul(r + i, rn - i, a, an, b[i]);
  }
  else if (bn + bn <= an + 6)
  {
    int n = (an + 1) / 2;
    r_addmul(r, rn, a, n, b, bn, t);
    r_addmul(r + n, rn - n, a + n, an - n, b, bn, t);
  }
  else
  {
    int n = (an + 1) / 2;
    int ts = 0;
    if (t == NULL)
    {
      ts = n * 5;
      t = new digit[ts];
    }
    digit* nt = t + n + n + 2;

    r_copy(t, a, n);
    t[n] = 0;
    r_add(t, n + 1, a + n, an - n);
    r_copy(t + n + 1, b, n);
    t[n + n + 1] = 0;
    r_add(t + n + 1, n + 1, b + n, bn - n);
    r_addmul(r + n, rn - n, t, n + 1, t + n + 1, n + 1, nt);

    r_zero(t, n + n);
    r_addmul(t, n + n, a, n, b, n, nt);
    r_sub(r + n, rn - n, t, n + n);
    r_add(r, rn, t, n + n);
    int t2 = _min(an + bn - n - n, rn - n);
    r_zero(t, t2);
    r_addmul(t, t2, a + n, an - n, b + n, bn - n, nt);
    r_sub(r + n, rn - n, t, t2);
    r_add(r + n + n, rn - n - n, t, t2);

    if (ts) delete[] t;
  }
}

Number* mul(Number* r, Number const* a, Number const* b)
{
  if (!(a->sign & 1) || !(b->sign & 1))
  {
    if (a->sign == 0 || b->sign == 0)
      return nan(r);
    if (a->sign & 1)
      return (a->len ? infinity(r, b->sign) : nan(r));
    if (b->sign & 1)
      return (b->len ? infinity(r, a->sign) : nan(r));
    return infinity(r, a->sign * b->sign);
  }
  digit* d;
  int len = a->len + b->len;
  if (r == a || r == b)
    d = new digit[len];
  else
    d = resize(r, len);
  r_zero(d, len);
  r_addmul(d, len, a->d, a->len, b->d, b->len);
  return fromptr(r, d, len, a->sign * b->sign, a->expo + b->expo);
}

int r_divmod(digit* a, int an, digit const* bc, int bn, digit* q, int qn)
{
  digit* b = const_cast<digit*>(bc);
  while (an && a[an - 1] == 0) an--;
  while (bn && b[bn - 1] == 0) bn--;

  if (bn == 0 || an < bn)
  {
    if (q) r_zero(q, qn);
    return (bn == 0);
  }

  if (q && an - bn + 1 < qn)
    r_zero(q + an - bn + 1, qn - an + bn - 1);

  if (bn == 1)
  {
    if (q)
      a[0] = r_div(q, a, an, b[0]);
    else
      a[0] = r_mod(a, an, b[0]);
    r_zero(a + 1, an - 1);
    return 0;
  }

  int shift = 0;
  while (((b[bn - 1] << shift) & high_bit) == 0)
    shift++;
  r_shl(b, b, bn, shift);
  digit extra = r_shl(a, a, an, shift);

  bigdigit b0 = b[bn - 1];
  bigdigit b1 = b[bn - 2];

  if (an >= bn)
  {
    bigdigit p = MAKEBIG(extra, a[an - 1]);
    bigdigit cd = p / b0;
    bigdigit cr = p - b0 * cd;
    if (cr < big_overflow && b1 * cd > MAKEBIG(cr, a[an - 2]))
    {
      cd--;
      cr += b0;
      if (cr < big_overflow && b1 * cd > MAKEBIG(cr, a[an - 2]))
        cd--;
    }
    digit c = r_smul(a + an - bn, bn, b, bn, digit(cd));
    if (extra < c)
    {
      cd--;
      r_add(a + an - bn, bn, b, bn);
    }
    if (q) q[an - bn] = digit(cd);
  }

  for (int i = an - bn - 1; i >= 0; i--)
  {
    bigdigit p = MAKEBIG(a[i + bn], a[i + bn - 1]);
    bigdigit cd = (a[i + bn] == b0 ? max_digit : p / b0);
    bigdigit cr = p - b0 * cd;

    if (cr < big_overflow && b1 * cd > MAKEBIG(cr, a[i + bn - 2]))
    {
      cd--;
      cr += b0;
      if (cr < big_overflow && b1 * cd > MAKEBIG(cr, a[i + bn - 2]))
        cd--;
    }

    if (r_smul(a + i, bn + 1, b, bn, digit(cd)))
    {
      cd--;
      r_add(a + i, bn + 1, b, bn);
    }

    if (q) q[i] = digit(cd);
  }

  r_shr(b, b, bn, shift);
  r_shr(a, a, an, shift);

  return 0;
}

Number* div(Number* q, Number const* a, Number const* b)
{
  if (!(a->sign & 1) || !(b->sign & 1) || b->len == 0)
  {
    if (a->sign == 0 || b->sign == 0 || (!(a->sign & 1) && !(b->sign & 1)))
      return nan(q);
    else
      return infinity(q, a->sign * b->sign);
  }

  int qbase = a->len - b->len + 1 + a->expo - b->expo;
  int qlen = qbase + max_exponent;
  if (qlen > max_length) qlen = max_length;
  if (qlen <= 0) return zero(q, a->sign * b->sign);
  int rlen = qlen + b->len - 1;

  digit* r = new digit[rlen];
  if (rlen > a->len)
  {
    r_zero(r, rlen - a->len);
    r_copy(r + rlen - a->len, a->d, a->len);
  }
  else
    r_copy(r, a->d + a->len - rlen, rlen);

  digit* qd;
  if (q == a || q == b)
    qd = resize(q, qlen);
  else
    qd = new digit[qlen];
  r_divmod(r, rlen, b->d, b->len, qd, qlen);

  delete[] r;

  return fromptr(q, qd, qlen, a->sign * b->sign, qbase - qlen);
}
Number* mod(Number* r, Number const* a, Number const* b)
{
  if (!(a->sign & 1) || !(b->sign & 1) || b->len == 0)
    return nan(r);

  int rexpo = _min(a->expo, b->expo);
  int rlen = a->len + a->expo - rexpo;

  digit* rd = ((r == a || r == b) ? new digit[rlen] : resize(r, rlen));

  if (rlen > a->len)
  {
    r_zero(rd, rlen - a->len);
    r_copy(rd + rlen - a->len, a->d, a->len);
  }
  else
    r_copy(rd, a->d + a->len - rlen, rlen);

  int qoffs = _max(b->expo - a->expo, 0);
  if (rlen > qoffs)
    r_divmod(rd + qoffs, rlen - qoffs, b->d, b->len);

  int sign = a->sign * b->sign;

  if (sign < 0 && !r_iszero(rd, rlen))
  {
    Number tr;
    fromptr(&tr, rd, rlen, sign, rexpo);
    rawsub(&tr, b, &tr);
    fromptr(r, tr.d, tr.len, 1, tr.expo);
    tr.m = NULL;
    return r;
  }
  else
    return fromptr(r, rd, rlen, 1, rexpo);
}
Number* divmod(Number* q, Number* r, Number const* a, Number const* b)
{
  if (!(a->sign & 1) || !(b->sign & 1) || b->len == 0)
  {
    if (a->sign == 0 || b->sign == 0 || (!(a->sign & 1) && !(b->sign & 1)))
      nan(q);
    else
      infinity(q, a->sign * b->sign);
    nan(r);
    return q;
  }

  int qlen = a->len - b->len + 1 + a->expo - b->expo;
  if (qlen <= 0)
  {
    if (a->sign > 0)
    {
      copy(r, a);
      return zero(q, a->sign * b->sign);
    }
    else
    {
      rawsub(r, b, a);
      fromdigit(q, 1, -b->sign);
      r->sign = 1;
      return q;
    }
  }

  int rexpo = _min(a->expo, b->expo);
  int rlen = a->len + a->expo - rexpo;

  digit* qd = ((q == a || q == b) ? new digit[qlen] : resize(q, qlen));
  digit* rd = ((r == a || r == b) ? new digit[rlen] : resize(r, rlen));

  if (rlen > a->len)
  {
    r_zero(rd, rlen - a->len);
    r_copy(rd + rlen - a->len, a->d, a->len);
  }
  else
    r_copy(rd, a->d + a->len - rlen, rlen);

  int qoffs = _max(b->expo - a->expo, 0);
  if (rlen > qoffs)
    r_divmod(rd + qoffs, rlen - qoffs, b->d, b->len, qd, qlen);
  else
    r_zero(qd, qlen);

  int sign = a->sign * b->sign;
  int qsign = sign;

  if (sign < 0 && !r_iszero(rd, rlen))
  {
    Number tr;
    fromptr(&tr, rd, rlen, sign, rexpo);
    rawsub(&tr, b, &tr);
    fromptr(r, tr.d, tr.len, 1, tr.expo);
    tr.m = NULL;

    if (a->sign == 1)
    {
      if (r_wsub(qd, qlen, 1))
      {
        qlen = 1;
        qd[0] = 1;
        qsign = -qsign;
      }
    }
    else
      r_wadd(qd, qlen, 1);
  }
  else
    fromptr(r, rd, rlen, 1, rexpo);
  return fromptr(q, qd, qlen, qsign, 0);
}

Number* sqrt(Number* r, Number const* a)
{
  if (a->sign == 2)
    return infinity(r);
  else if (a->sign != 1)
    return nan(r);
  if (a->len + a->expo + max_exponent * 2 < 0)
    return zero(r);

  max_exponent++;

  int len = a->len + a->expo + max_exponent * 2;
  digit* cur = new digit[len];
  digit* rem = new digit[len];
  digit* rel = new digit[len];
  r_zero(cur, len);
  cur[len / 2] = 1;
  int offs = a->expo + max_exponent * 2;

  while (true)
  {
    if (offs >= 0)
    {
      r_zero(rem, offs);
      r_copy(rem + offs, a->d, a->len);
    }
    else
      r_copy(rem, a->d - offs, a->len + offs);
    if (r_divmod(rem, len, cur, len, rel, len))
      break;
    if (r_compare(cur + 1, rel + 1, len - 1) == 0)
      break;
    digit c = r_add(cur, len, rel, len);
    r_shr(cur, cur, len, 1, c);
  }

  max_exponent--;
  delete[] rem;
  delete[] rel;
  
  return fromptr(r, cur, len, 1, -max_exponent - 1);
}

Number* pow(Number* r, Number const* a, Number const* b)
{
  if (a->sign == 0 || b->sign == 0)
    return nan(r);
  if ((b->sign & 1) && b->len == 0)
    return fromdigit(r, 1);
  if (!(a->sign & 1) || !(b->sign & 1))
  {
    if (a->sign == Sign::PosInfinity)
    {
      if (b->sign > 0) return infinity(r);
      return zero(r);
    }
    if (a->sign == Sign::NegInfinity)
    {
      if (b->sign > 0) return nan(r);
      return zero(r);
    }
    bool az = ((a->sign & 1) != 0 && (a->len == 0 || a->len + a->expo <= 0));
    bool ao = ((a->sign & 1) != 0 && a->len == 1 && a->expo == 0 && a->d[0] == 1);
    if (b->sign == Sign::NegInfinity)
    {
      if (a->sign < 0) return nan(r);
      if (az) return infinity(r);
      if (ao) return fromdigit(r, 1);
      return zero(r);
    }
    if (b->sign == Sign::PosInfinity)
    {
      if (az) return zero(r);
      if (ao) return fromdigit(r, 1);
      if (a->sign < 0) return nan(r);
      return infinity(r);
    }
  }

  int neg = negbits(b);
  if (neg && a->sign < 0)
    return nan(r);

  Number A, B;
  copy(&A, a);
  if (neg)
  {
    max_exponent++;
    int len = _max(a->len + a->expo, 0) + max_exponent * 2;
    digit* cpw = new digit[len];
    digit* cbb[2];
    int cbi = -1;
    cbb[0] = new digit[len];
    cbb[1] = new digit[len];
    digit* cur = new digit[len];
    digit* rem = new digit[len];
    digit* rel = new digit[len];
    digit* mtt = new digit[r_multempsize(len - max_exponent)];

    int clen = a->expo + max_exponent * 2;
    if (clen >= 0)
    {
      r_zero(cpw, clen);
      r_copy(cpw + clen, a->d, a->len);
    }
    else
      r_copy(cpw, a->d - clen, a->len + clen);
    if (a->len + a->expo < 0)
      r_zero(cpw + clen + a->len, -a->expo - a->len);
    clen += a->len;

    for (int i = 1; i <= neg; i++)
    {
      r_zero(cur, clen);
      cur[clen / 2] = 1;

      while (true)
      {
        r_copy(rem, cpw, clen);
        if (r_divmod(rem, clen, cur, clen, rel, clen))
          break;
        if (r_compare(cur + 1, rel + 1, clen) == 0 && (cur[0] - rel[0] < 256 || rel[0] - cur[0] < 256))
          break;
        digit c = r_add(cur, clen, rel, clen);
        r_shr(cur, cur, clen, 1, c);
      }

      r_zero(cpw, max_exponent);
      r_copy(cpw + max_exponent, cur, _min(clen, len - max_exponent));
      clen = _min(clen + max_exponent, len);
      while (clen && cpw[clen - 1] == 0) clen--;

      if (getbit(b, -i))
      {
        if (cbi < 0)
        {
          r_copy(cbb[0], cpw, clen);
          r_zero(cbb[0] + clen, len - clen);
          cbi = 0;
        }
        else
        {
          r_zero(cbb[1 - cbi], len);
          r_addmul(cbb[1 - cbi], len, cbb[cbi] + max_exponent, len - max_exponent,
                                          cpw + max_exponent, clen - max_exponent, mtt);
          cbi = 1 - cbi;
        }
      }
    }

    max_exponent--;
    fromptr(&B, cbb[cbi], len, 1, -max_exponent * 2 - 2);
    delete[] cpw; delete[] cbb[1 - cbi]; delete[] mtt;
    delete[] cur; delete[] rem; delete[] rel;
  }
  else
    fromdigit(&B, 1);

  int pos = bits(b);
  for (int i = 0; i < pos; i++)
  {
    if (getbit(b, i))
      mul(&B, &A, &B);
    if (i < pos - 1)
      mul(&A, &A, &A);
  }

  if (b->sign < 0)
  {
    fromdigit(&A, 1);
    div(&B, &A, &B);
  }

  fromptr(r, B.d, B.len, B.sign, B.expo);
  B.m = NULL;
  return r;
}

Number* shift(Number* r, Number const* a, int left)
{
  if (!(a->sign & 1) || a->len == 0) return zero(r, a->sign);

  if (left > 0)
  {
    digit* d = (r == a ? new digit[a->len + 1] : resize(r, a->len + 1));
    d[a->len] = r_shl(d, a->d, a->len, left % digit_size);
    return fromptr(r, d, a->len + 1, a->sign, a->expo + (left / digit_size));
  }
  else if (left < 0)
  {
    int right = -left;
    digit* d = (r == a ? new digit[a->len + 1] : resize(r, a->len + 1));
    d[0] = r_shr(d + 1, a->d, a->len, right % digit_size);
    return fromptr(r, d, a->len + 1, a->sign, a->expo - (right / digit_size));
  }
  else
    return copy(r, a);
}
static inline int notrunc(Number const* a)
{
  return (!(a->sign & 1) || a->expo >= 0 || a->len == 0);
}
static Number* dotrunc(Number* r, Number const* a, int up)
{
  if (r == a)
    expand(r, _min(a->len + a->expo, 0) + up, 0);
  else
  {
    resize(r, _min(a->len + a->expo, 0) + up);
    r->sign = a->sign;
    r->expo = 0;
    copydigits(r, a);
  }
  if (up) r_wadd(r->d, r->len, 1);
  trim(r);
  if (r->len == 0) r->sign = 1;
  return r;
}
Number* floor(Number* r, Number const* a)
{
  if (notrunc(a)) return copy(r, a);
  return dotrunc(r, a, a->sign < 0);
}
Number* ceil(Number* r, Number const* a)
{
  if (notrunc(a)) return copy(r, a);
  return dotrunc(r, a, a->sign > 0);
}
Number* round(Number* r, Number const* a)
{
  if (notrunc(a)) return copy(r, a);
  if (a->len + a->expo < 0) return zero(r);
  if (a->d[-a->expo - 1] < high_bit) return dotrunc(r, a, 0);
  if (a->d[-a->expo - 1] > high_bit) return dotrunc(r, a, 1);
  return dotrunc(r, a, a->sign > 0 || a->expo < -1);
}

}
