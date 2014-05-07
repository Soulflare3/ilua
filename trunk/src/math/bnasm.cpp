#include "bnimpl.h"

namespace bn
{

digit r_wadd(digit* r, int rn, digit a)
{
  if (rn < 0) return a;
  _asm
  {
    mov edi, dword ptr[r]
    mov eax, dword ptr[a]
    mov ecx, dword ptr[rn]
    cmp ecx, 0
    jz  allfinish
carryloop:
    add dword ptr[edi], eax
    jnc nocarry
    mov eax, 1
    add edi, 4
    dec ecx
    jnz carryloop
    jmp allfinish
nocarry:
    xor eax, eax
allfinish:
  }
}
digit r_wsub(digit* r, int rn, digit a)
{
  if (rn < 0) return a;
  _asm
  {
    mov edi, dword ptr[r]
    mov eax, dword ptr[a]
    mov ecx, dword ptr[rn]
    cmp ecx, 0
    jz  allfinish
carryloop:
    sub dword ptr[edi], eax
    jnc nocarry
    mov eax, 1
    add edi, 4
    dec ecx
    jnz carryloop
    jmp allfinish
nocarry:
    xor eax, eax
allfinish:
  }
}
digit r_add(digit* r, int rn, digit const* a, int an)
{
  rn -= an;
  if (rn < 0) an += rn, rn = 0;
  if (an < 0) an = 0;
  _asm
  {
    cld
    mov esi, dword ptr[a]
    mov edi, dword ptr[r]
    mov ecx, dword ptr[an]
    mov ebx, 0
    cmp ecx, 4
    jl  addfinish4
addloop4:
    neg ebx
    lodsd
    adc eax, dword ptr[edi]
    stosd
    lodsd
    adc eax, dword ptr[edi]
    stosd
    lodsd
    adc eax, dword ptr[edi]
    stosd
    lodsd
    adc eax, dword ptr[edi]
    stosd
    sbb ebx, ebx
    sub ecx, 4
    cmp ecx, 4
    jge addloop4
addfinish4:

    cmp ecx, 0
    jz  addfinish
addloop:
    neg ebx
    lodsd
    adc eax, dword ptr[edi]
    stosd
    sbb ebx, ebx
    dec ecx
    jnz  addloop
addfinish:

    neg ebx
    jnc allfinish
    mov ecx, dword ptr[rn]
    cmp ecx, 0
    jz  allfinish
carryloop:
    add dword ptr[edi], 1
    jnc nocarry
    add edi, 4
    dec ecx
    jnz carryloop
    jmp allfinish
nocarry:
    xor ebx, ebx
allfinish:
    mov eax, ebx
  }
}
digit r_sub(digit* r, int rn, digit const* a, int an)
{
  rn -= an;
  if (rn < 0) an += rn, rn = 0;
  if (an < 0) an = 0;
  _asm
  {
    mov esi, dword ptr[a]
    mov edi, dword ptr[r]
    mov ecx, dword ptr[an]
    mov eax, 0
    cmp ecx, 4
    jl  addfinish4
addloop4:
    neg eax
    mov ebx, dword ptr[esi]
    sbb dword ptr[edi], ebx
    mov ebx, dword ptr[esi+4]
    sbb dword ptr[edi+4], ebx
    mov ebx, dword ptr[esi+8]
    sbb dword ptr[edi+8], ebx
    mov ebx, dword ptr[esi+12]
    sbb dword ptr[edi+12], ebx
    sbb eax, eax
    add esi, 16
    add edi, 16
    sub ecx, 4
    cmp ecx, 4
    jge addloop4
addfinish4:

    cmp ecx, 0
    jz  addfinish
addloop:
    neg eax
    mov ebx, dword ptr[esi]
    sbb dword ptr[edi], ebx
    sbb eax, eax
    add esi, 4
    add edi, 4
    dec ecx
    jnz  addloop
addfinish:

    neg eax
    jnc allfinish
    mov ecx, dword ptr[rn]
    cmp ecx, 0
    jz  allfinish
carryloop:
    sub dword ptr[edi], 1
    jnc nocarry
    add edi, 4
    dec ecx
    jnz carryloop
    jmp allfinish
nocarry:
    xor eax, eax
allfinish:
  }
}
digit r_mul(digit* r, int rn, digit const* a, int an, digit b, digit c)
{
  bigdigit x;
  rn -= an;
  if (rn < 0) an += rn, rn = 0;
  if (an < 0) an = 0;
  digit t;
  while (an&~3)
  {
    x = bigdigit(a[0]) * bigdigit(b) + bigdigit(c);
    r[0] = digit(x);
    c = digit(x >> digit_size);

    x = bigdigit(a[1]) * bigdigit(b) + bigdigit(c);
    r[1] = digit(x);
    c = digit(x >> digit_size);

    x = bigdigit(a[2]) * bigdigit(b) + bigdigit(c);
    r[2] = digit(x);
    c = digit(x >> digit_size);

    x = bigdigit(a[3]) * bigdigit(b) + bigdigit(c);
    r[3] = digit(x);
    c = digit(x >> digit_size);

    r += 4, a += 4, an -= 4;
  }
  while (an)
  {
    x = bigdigit(*a) * bigdigit(b) + bigdigit(c);
    *r = digit(x);
    c = digit(x >> digit_size);
    r++, a++, an--;
  }
  while (c && rn)
  {
    t = *r + c;
    c = (t < c);
    *r = t;
    r++, rn--;
  }
  return c;
}
digit r_amul(digit* r, int rn, digit const* a, int an, digit b)
{
  bigdigit x;
  rn -= an;
  if (rn < 0) an += rn, rn = 0;
  if (an < 0) an = 0;
  digit c = 0, t;
  while (an&~3)
  {
    x = bigdigit(a[0]) * bigdigit(b) + bigdigit(r[0]) + bigdigit(c);
    r[0] = digit(x);
    c = digit(x >> digit_size);

    x = bigdigit(a[1]) * bigdigit(b) + bigdigit(r[1]) + bigdigit(c);
    r[1] = digit(x);
    c = digit(x >> digit_size);

    x = bigdigit(a[2]) * bigdigit(b) + bigdigit(r[2]) + bigdigit(c);
    r[2] = digit(x);
    c = digit(x >> digit_size);

    x = bigdigit(a[3]) * bigdigit(b) + bigdigit(r[3]) + bigdigit(c);
    r[3] = digit(x);
    c = digit(x >> digit_size);

    r += 4, a += 4, an -= 4;
  }
  while (an)
  {
    x = bigdigit(*a) * bigdigit(b) + bigdigit(*r) + bigdigit(c);
    *r = digit(x);
    c = digit(x >> digit_size);
    r++, a++, an--;
  }
  while (c && rn)
  {
    t = *r + c;
    c = (t < c);
    *r = t;
    r++, rn--;
  }
  return c;
}
digit r_smul(digit* r, int rn, digit const* a, int an, digit b)
{
  bigdigit x;
  rn -= an;
  if (rn < 0) an += rn, rn = 0;
  if (an < 0) an = 0;
  digit c = 0, t;
  while (an&~3)
  {
    x = bigdigit(r[0]) - bigdigit(a[0]) * bigdigit(b) - bigdigit(c);
    r[0] = digit(x);
    c = -digit(x >> digit_size);

    x = bigdigit(r[1]) - bigdigit(a[1]) * bigdigit(b) - bigdigit(c);
    r[1] = digit(x);
    c = -digit(x >> digit_size);

    x = bigdigit(r[2]) - bigdigit(a[2]) * bigdigit(b) - bigdigit(c);
    r[2] = digit(x);
    c = -digit(x >> digit_size);

    x = bigdigit(r[3]) - bigdigit(a[3]) * bigdigit(b) - bigdigit(c);
    r[3] = digit(x);
    c = -digit(x >> digit_size);

    r += 4, a += 4, an -= 4;
  }
  while (an)
  {
    x = bigdigit(r[0]) - bigdigit(a[0]) * bigdigit(b) - bigdigit(c);
    r[0] = digit(x);
    c = -digit(x >> digit_size);
    r++, a++, an--;
  }
  while (c && rn)
  {
    t = *r;
    *r = t - c;
    c = (t == 0);
    r++, rn--;
  }
  return c;
}

digit r_div(digit* r, digit const* a, int n, digit b)
{
  _asm
  {
    std
    mov esi, dword ptr[a]
    mov edi, dword ptr[r]
    mov ecx, dword ptr[n]
    mov ebx, dword ptr[b]

    mov eax, ecx
    dec eax
    shl eax, 2
    add esi, eax
    add edi, eax

    mov edx, 0
    cmp ecx, 4
    jl  divfinish4
divloop4:
    lodsd
    div ebx
    stosd
    lodsd
    div ebx
    stosd
    lodsd
    div ebx
    stosd
    lodsd
    div ebx
    stosd

    sub ecx, 4
    cmp ecx, 4
    jge divloop4
divfinish4:

    cmp ecx, 0
    jz  divfinish
divloop:
    lodsd
    div ebx
    stosd

    dec ecx
    jnz  divloop
divfinish:
    mov eax, edx
    cld
  }
}

digit r_mod(digit const* a, int n, digit b)
{
  _asm
  {
    std
    mov esi, dword ptr[a]
    mov ecx, dword ptr[n]
    mov ebx, dword ptr[b]

    mov eax, ecx
    dec eax
    shl eax, 2
    add esi, eax

    mov edx, 0
    cmp ecx, 4
    jl  divfinish4
divloop4:
    lodsd
    div ebx
    lodsd
    div ebx
    lodsd
    div ebx
    lodsd
    div ebx

    sub ecx, 4
    cmp ecx, 4
    jge divloop4
divfinish4:

    cmp ecx, 0
    jz  divfinish
divloop:
    lodsd
    div ebx

    dec ecx
    jnz  divloop
divfinish:
    mov eax, edx
    cld
  }
}
digit r_shl(digit* r, digit const* a, int n, int s, digit c)
{
  if (s == 0)
  {
    if (r != a) r_copy(r, a, n);
    return 0;
  }
  _asm
  {
    cld
    mov esi, dword ptr[a]
    mov edi, dword ptr[r]
    mov ecx, dword ptr[s]
    mov ebx, dword ptr[c]
    mov edx, dword ptr[n]
    cmp edx, 4
    jl  shlfinish4
shlloop4:
    lodsd
    mov edx, eax
    shld eax, ebx, cl
    stosd

    lodsd
    mov ebx, eax
    shld eax, edx, cl
    stosd

    lodsd
    mov edx, eax
    shld eax, ebx, cl
    stosd

    lodsd
    mov ebx, eax
    shld eax, edx, cl
    stosd

    mov edx, dword ptr[n]
    sub edx, 4
    mov dword ptr[n], edx
    cmp edx, 4
    jge shlloop4
shlfinish4:

    cmp edx, 0
    jz  shlfinish
shlloop:
    lodsd
    mov edx, eax
    shld eax, ebx, cl
    stosd
    mov ebx, edx

    dec dword ptr[n]
    jnz  shlloop
shlfinish:
    xor eax, eax
    shld eax, ebx, cl
  }
}
digit r_shr(digit* r, digit const* a, int n, int s, digit c)
{
  if (s == 0)
  {
    if (r != a) r_copy(r, a, n);
    return 0;
  }
  _asm
  {
    std
    mov esi, dword ptr[a]
    mov edi, dword ptr[r]
    mov ecx, dword ptr[s]
    mov ebx, dword ptr[c]
    mov eax, dword ptr[n]
    mov edx, eax
    dec eax
    shl eax, 2
    add esi, eax
    add edi, eax
    cmp edx, 4
    jl  shrfinish4
shrloop4:
    lodsd
    mov edx, eax
    shrd eax, ebx, cl
    stosd

    lodsd
    mov ebx, eax
    shrd eax, edx, cl
    stosd

    lodsd
    mov edx, eax
    shrd eax, ebx, cl
    stosd

    lodsd
    mov ebx, eax
    shrd eax, edx, cl
    stosd

    mov edx, dword ptr[n]
    sub edx, 4
    mov dword ptr[n], edx
    cmp edx, 4
    jge shrloop4
shrfinish4:

    cmp edx, 0
    jz  shrfinish
shrloop:
    lodsd
    mov edx, eax
    shrd eax, ebx, cl
    stosd
    mov ebx, edx

    dec dword ptr[n]
    jnz  shrloop
shrfinish:
    cld
    xor eax, eax
    shrd eax, ebx, cl
  }
}

}
