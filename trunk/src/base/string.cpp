#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <locale>
#include <windows.h>
#include <winuser.h>

#include "string.h"
#include "base/utf8.h"

#define ENABLE_UTF8

#define BASIC_ALLOC         16
#define ALLOC_STEP          1024

static char __empty[] = {0, 0, 0, 0,
                         1, 0, 0, 0,
                         1, 0, 0, 0,
                         0};

String String::null;

String::String()
{
  buf = __empty + 12;
  _ref(buf)++;
}

String::String(char const* str)
{
  if (str)
  {
    int len = strlen(str);
    buf = _new(len + 1);
    _len(buf) = len;
    _ref(buf) = 1;
    memcpy(buf, str, len + 1);
  }
  else
  {
    buf = __empty + 12;
    _ref(buf)++;
  }
}

String::String(char const* str, int length)
{
  buf = _new(length + 1);
  _len(buf) = length;
  _ref(buf) = 1;
  memcpy(buf, str, length);
  buf[length] = 0;
}

String::String(char value)
{
  buf = _new(2);
  _len(buf) = 1;
  _ref(buf) = 1;
  buf[0] = value;
  buf[1] = 0;
}

String::String(String const& str)
  : buf(str.buf)
{
  _ref(buf)++;
}

String::String(int value)
{
  buf = _new(BASIC_ALLOC);
  _ref(buf) = 1;
  itoa(value, buf, 10);
  _len(buf) = strlen(buf);
}

String::String(float value)
{
  buf = _new(BASIC_ALLOC);
  _ref(buf) = 1;
  sprintf(buf, "%f", value);
  _len(buf) = strlen(buf);
}
String::String(double value)
{
  buf = _new(BASIC_ALLOC);
  _ref(buf) = 1;
  sprintf(buf, "%lf", value);
  _len(buf) = strlen(buf);
}

String& String::operator = (String const& str)
{
  if (&str == this)
    return *this;

  deref();
  buf = str.buf;
  if (buf)
    _ref(buf)++;
  return *this;
}
String& String::operator = (char const* str)
{
  if (str == buf)
    return *this;

  if (str)
  {
    if (_ref(buf) > 1) splice();
    int len = strlen(str);
    realloc(len);
    memcpy(buf, str, len + 1);
    _len(buf) = len;
  }
  else
  {
    deref();
    buf = __empty + 12;
    _ref(buf)++;
  }
  return *this;
}

String& String::operator += (char ch)
{
  if (_ref(buf) > 1) splice();
  if (_len(buf) + 1 >= _size(buf))
    realloc(_len(buf) + 1);

  buf[_len(buf)++] = ch;
  buf[_len(buf)] = 0;

  return *this;
}

String& String::operator *= (int n)
{
  if (n == 1)
    return *this;
  if (_ref(buf) > 1) splice();
  if (_len(buf) * n >= _size(buf))
    realloc(_len(buf) * n);

  if (n <= 0)
  {
    _len(buf) = 0;
    buf[0] = 0;
    return *this;
  }

  int len = _len(buf);
  int pos = 0;
  for (int i = 1; i < n; i++, pos += len)
    memcpy(buf + pos + len, buf + pos, len);
  buf[_len(buf) *= n] = 0;

  return *this;
}

int String::toInt() const
{
  return atoi(buf);
}
bool String::isInt() const
{
  int i = 0;
  while (buf[i] == ' ' || buf [i] == '\t')
    i++;
  if (buf[i] == '+' || buf[i] == '-')
    i++;
  return (buf[i] >= '0' && buf[i] <= '9');
}

float String::toFloat() const
{
  return (float) atof(buf);
}
double String::toDouble() const
{
  return atof(buf);
}

bool String::isFloat() const
{
  int i = 0;
  while (buf[i] == ' ' || buf[i] == '\t')
    i++;
  if (buf[i] == '+' || buf[i] == '-')
    i++;
  int count = 0;
  while (buf[i] >= '0' && buf[i] <= '9')
    i++, count++;
  if (buf[i] == '.')
  {
    i++;
    count = 0;
    while (buf[i] >= '0' && buf[i] <= '9')
      i++, count++;
  }
  if (buf[i] == 'e' || buf[i] == 'E' || buf[i] == 'd' || buf[i] == 'D')
  {
    i++;
    if (buf[i] == '+' || buf[i] == '-')
      i++;
    while (buf[i] >= '0' && buf[i] <= '9')
      i++;
  }
  return (count != 0);
}

void String::splice()
{
  char* tmp = _new(_size(buf));
  _len(tmp) = _len(buf);
  _ref(tmp) = 1;
  memcpy(tmp, buf, _len(buf) + 1);
  deref();
  buf = tmp;
}

void String::realloc(int newlen)
{
  newlen++;
  if (newlen <= _size(buf))
    return;
  int newsize = (_size(buf) < ALLOC_STEP ? _size(buf) * 2 : _size(buf) + ALLOC_STEP);
  if (newsize < newlen)
    newsize = newlen;
  char* tmp = _new(newsize);
  _len(tmp) = _len(buf);
  _ref(tmp) = _ref(buf);
  memcpy(tmp, buf, _len(buf) + 1);
  _del(buf);
  buf = tmp;
}

String& String::append(char const* str, int len)
{
  if (_ref(buf) > 1) splice();
  if (len < 0) len = strlen(str);
  realloc(_len(buf) + len);
  memcpy(buf + _len(buf), str, len);
  _len(buf) += len;
  buf[_len(buf)] = 0;
  return *this;
}

String String::substr(int from, int len) const
{
  if (from < 0) from += _len(buf);
  if (from < 0) from = 0;
  if (from > _len(buf)) from = _len(buf);
  if (len == toTheEnd) len = _len(buf) - from;
  else if (len < 0) len += _len(buf) - from;
  else if (from + len > _len(buf)) len = _len(buf) - from;
  if (len < 0) len = 0;
  char* str = _new(len + 1);
  _len(str) = len;
  _ref(str) = 1;
  if (len)
    memcpy(str, buf + from, len);
  str[len] = 0;
  return String((void*) str);
}

String String::substring(int from, int to) const
{
  if (from < 0) from += _len(buf);
  if (from < 0) from = 0;
  if (from > _len(buf)) from = _len(buf);
  if (to < 0) to += _len(buf);
  else if (to == toTheEnd) to = _len(buf);
  else if (to > _len(buf)) to = _len(buf);
  int len = to - from;
  if (len < 0) len = 0;
  char* str = _new(len + 1);
  _len(str) = len;
  _ref(str) = 1;
  if (len)
    memcpy(str, buf + from, len);
  str[len] = 0;
  return String((void*) str);
}

String& String::resize(int newlen, char c)
{
  if (_ref(buf) > 1) splice();
  if (newlen > _len(buf))
  {
    realloc(newlen);
    memset(buf + _len(buf), c, newlen - _len(buf));
  }
  _len(buf) = newlen;
  buf[newlen] = 0;
  return *this;
}

String& String::removeLeadingSpaces()
{
  int i = 0;
  while (buf[i] && s_isspace(buf[i]))
    i++;
  if (i)
  {
    if (_ref(buf) > 1) splice();
    _len(buf) -= i;
    memmove(buf, buf + i, _len(buf) + 1);
  }
  return *this;
}

String& String::removeTrailingSpaces()
{
  int i = _len(buf);
  while (i > 0 && s_isspace(buf[i - 1]))
    i--;
  if (i < _len(buf))
  {
    if (_ref(buf) > 1) splice();
    buf[_len(buf) = i] = 0;
  }
  return *this;
}

String& String::trim()
{
  removeTrailingSpaces();
  removeLeadingSpaces();
  return *this;
}

String& String::dequote()
{
  if (_len(buf) >= 2 && (
    (buf[0] == '\"' && buf[_len(buf) - 1] == '\"') ||
    (buf[0] == '\'' && buf[_len(buf) - 1] == '\'')))
  {
    if (_ref(buf) > 1) splice();
    memmove(buf, buf + 1, _len(buf) -= 2);
    buf[_len(buf)] = 0;
  }
  return *this;
}

String& String::cut(int from, int len)
{
  if (from < 0) from += _len(buf);
  if (from < 0) from = 0;
  if (from > _len(buf)) from = _len(buf);
  if (len == toTheEnd) len = _len(buf) - from;
  else if (len < 0) len += _len(buf) - from;
  else if (from + len > _len(buf)) len = _len(buf) - from;
  if (len > 0)
  {
    if (_ref(buf) > 1) splice();
    if (from + len < _len(buf))
      memmove(buf + from, buf + from + len, _len(buf) - from - len);
    buf[_len(buf) -= len] = 0;
  }
  return *this;
}

bool String::isWordBoundary(int pos) const
{
  return pos <= 0 || pos >= _len(buf) || isIdChar(buf[pos]) != isIdChar(buf[pos - 1]);
}
int String::find(char const* str, int start) const
{
  static int kmpbuf[256];
  if (start < 0) start += _len(buf);
  if (start < 0) start = 0;
  if (start > _len(buf)) start = _len(buf);
  if (*str == 0) return start;
  int len = strlen(str);
  int* kmp = kmpbuf;
  if (len > 256)
    kmp = new int[len];
  kmp[0] = 0;
  int result = -1;
  for (int i = 1; i < len; i++)
  {
    kmp[i] = kmp[i - 1];
    while (kmp[i] && str[kmp[i]] != str[i])
      kmp[i] = kmp[kmp[i] - 1];
    if (str[kmp[i]] == str[i])
      kmp[i]++;
  }
  int cur = 0;
  for (int i = start; i < _len(buf); i++)
  {
    while (cur && str[cur] != buf[i])
      cur = kmp[cur - 1];
    if (str[cur] == buf[i])
      cur++;
    if (cur == len)
    {
      result = i - len + 1;
      break;
    }
  }
  if (kmp != kmpbuf)
    delete[] kmp;
  return result;
}
int String::ifind(char const* str, int start) const
{
  if (start < 0) start += _len(buf);
  if (start < 0) start = 0;
  if (start > _len(buf)) start = _len(buf);
  if (*str == 0) return start;
  // just brute force it
  uint8_const_ptr self = (uint8_const_ptr) buf + start;
  uint8_const_ptr other = (uint8_const_ptr) str;
  uint32 cp0 = utf8::transform(&other, utf8::tf_lower);
  while (*self)
  {
    int pos = (self - (uint8_const_ptr) buf);
    if (utf8::transform(&self, utf8::tf_lower) == cp0)
    {
      uint8_const_ptr a = self;
      uint8_const_ptr b = other;
      bool found = true;
      while (*b && found)
      {
        if (utf8::transform(&a, utf8::tf_lower) !=
            utf8::transform(&b, utf8::tf_lower))
          found = false;
      }
      if (found)
        return pos;
    }
  }
  return -1;
}

int String::find(char ch, int start) const
{
  for (int i = start; buf[i]; i++)
    if (buf[i] == ch)
      return i;
  return -1;
}
int String::ifind(char ch, int start) const
{
  ch = s_tolower(ch);
  for (int i = start; buf[i]; i++)
    if (s_tolower(buf[i]) == ch)
      return i;
  return -1;
}
int String::indexOf(char ch) const
{
  for (int i = 0; buf[i]; i++)
    if (buf[i] == ch)
      return i;
  return -1;
}
int String::lastIndexOf(char ch) const
{
  for (int i = _len(buf) - 1; i >= 0; i--)
    if (buf[i] == ch)
      return i;
  return -1;
}

String& String::replace(int pos, char ch)
{
  if (_ref(buf) > 1) splice();
  if (pos < 0) pos += _len(buf);
  if (pos < 0) pos = 0;
  if (pos > _len(buf)) pos = _len(buf);
  if (pos == _len(buf))
    return *this += ch;
  buf[pos] = ch;
  return *this;
}
String& String::replace(int start, int len, char const* str)
{
  if (_ref(buf) > 1) splice();
  int strLength = strlen(str);
  if (start < 0) start += _len(buf);
  if (start < 0) start = 0;
  if (start > _len(buf)) start = _len(buf);
  if (len == toTheEnd) len = _len(buf) - start;
  else if (len < 0) len += _len(buf) - start;
  else if (start + len > _len(buf)) len = _len(buf) - start;
  if (len < 0) len = 0;
  int newlen = _len(buf) - len + strLength;
  realloc(newlen);
  int b = start + len;
  int bnew = start + strLength;
  if (b < _len(buf))
    memmove(buf + bnew, buf + b, _len(buf) - b);
  if (strLength)
    memcpy(buf + start, str, strLength);
  buf[newlen] = 0;
  _len(buf) = newlen;
  return *this;
}

String& String::insert(int pos, char ch)
{
  if (_ref(buf) > 1) splice();
  if (pos < 0) pos += _len(buf);
  if (pos < 0) pos = 0;
  if (pos > _len(buf)) pos = _len(buf);
  realloc(_len(buf) + 1);
  if (pos < _len(buf))
    memmove(buf + pos + 1, buf + pos, _len(buf) - pos);
  buf[pos] = ch;
  buf[++_len(buf)] = 0;
  return *this;
}

String& String::insert(int pos, char const* str)
{
  if (_ref(buf) > 1) splice();
  if (pos < 0) pos += _len(buf);
  if (pos < 0) pos = 0;
  if (pos > _len(buf)) pos = _len(buf);
  int strLength = strlen(str);
  realloc(_len(buf) + strLength);
  if (pos < _len(buf))
    memmove(buf + pos + strLength, buf + pos, _len(buf) - pos);
  if (strLength)
    memcpy(buf + pos, str, strLength);
  buf[_len(buf) += strLength] = 0;
  return *this;
}

bool String::isAlpha() const
{
  for (int i = 0; i < _len(buf); i++)
    if (!s_isalpha((unsigned char) buf[i]))
      return false;
  return true;
}

bool String::isAlNum() const
{
  for (int i = 0; i < _len(buf); i++)
    if (!s_isalnum((unsigned char) buf[i]))
      return false;
  return true;
}

bool String::isDigits() const
{
  for (int i = 0; i < _len(buf); i++)
    if (!s_isdigit((unsigned char) buf[i]))
      return false;
  return true;
}

bool String::isHexDigits() const
{
  for (int i = 0; i < _len(buf); i++)
    if (!s_isxdigit((unsigned char) buf[i]))
      return false;
  return true;
}

String String::getExtension(String str)
{
  for (int i = _len(str.buf) - 1; i >= 0; i--)
  {
    if (str.buf[i] == '/' || str.buf[i] == '\\')
      return "";
    if (str.buf[i] == '.')
      return str.substr(i);
  }
  return "";
}

void String::setExtension(String& str, String ext)
{
  for (int i = _len(str.buf) - 1; i >= 0; i--)
  {
    if (str.buf[i] == '/' || str.buf[i] == '\\')
      break;
    if (str.buf[i] == '.')
    {
      str.replace(i, toTheEnd, ext);
      return;
    }
  }
  str += ext;
}

bool String::isRelPath(String str)
{
  return str.find(':') < 0;
}

String String::getPath(String str)
{
  for (int i = _len(str.buf) - 1; i >= 0; i--)
    if (str.buf[i] == '/' || str.buf[i] == '\\')
      return str.substr(0, i + 1);
  return "";
}

String String::getFileName(String str)
{
  for (int i = _len(str.buf) - 1; i >= 0; i--)
    if (str.buf[i] == '/' || str.buf[i] == '\\')
      return str.substr(i + 1);
  return str;
}

String String::getFileTitle(String str)
{
  int dot = toTheEnd;
  int i = _len(str.buf) - 1;
  if (str.buf[i] == '/' || str.buf[i] == '\\')
    dot = i--;
  for (; i >= 0; i--)
  {
    if (str.buf[i] == '/' || str.buf[i] == '\\')
      return str.substring(i + 1, dot);
    if (str.buf[i] == '.' && dot == toTheEnd)
      dot = i;
  }
  return str.substring(0, dot);
}

String String::buildFullName(String path, String name)
{
  if (_len(path.buf) == 0)
    return name;
  else if (path.buf[_len(path.buf) - 1] == '/' || path.buf[_len(path.buf) - 1] == '\\')
    return path + name;
  else
    return path + '\\' + name;
}

bool String::isWhitespace(unsigned char c)
{
  return s_isspace(c) != 0;
}
bool String::isIdChar(unsigned char c)
{
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool String::isIdStart(unsigned char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool String::isIdentifier(String str)
{
  if (_len(str.buf) == 0)
    return false;
  if (!isIdStart(str.buf[0]))
    return false;
  for (int i = 1; i < _len(str.buf); i++)
    if (!isIdChar(str.buf[i]))
      return false;
  return true;
}

String String::format(char const* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  char* buf = _new(_vscprintf(fmt, ap) + 1);
  _len(buf) = _size(buf) - 1;
  _ref(buf) = 1;
  vsprintf(buf, fmt, ap);
  return String((void*) buf);
}
String& String::printf(char const* fmt, ...)
{
  if (_ref(buf) > 1) splice();

  va_list ap;
  va_start(ap, fmt);

  int len = _vscprintf(fmt, ap);
  realloc(_len(buf) + len);
  vsprintf(buf + _len(buf), fmt, ap);
  _len(buf) += len;

  return *this;
}

void String::parseString(String str, String& cmd, String& args)
{
  int pos = 0;
  while (str.buf[pos] != 0 && (str.buf[pos] == ' ' || str.buf[pos] == '\t'))
    pos++;
  int cmdstart, cmdend;
  if (str.buf[pos] == '\'')
  {
    cmdstart = ++pos;
    while (str.buf[pos] != 0 && str.buf[pos] != '\'')
      pos++;
    cmdend = pos++;
  }
  else if (str.buf[pos] == '"')
  {
    cmdstart = ++pos;
    while (str.buf[pos] != 0 && str.buf[pos] != '"')
      pos++;
    cmdend = pos++;
  }
  else
  {
    cmdstart = pos;
    while (str.buf[pos] != 0 && str.buf[pos] != ' ' && str.buf[pos] != '\t')
      pos++;
    cmdend = pos;
  }
  cmd = str.substring(cmdstart, cmdend);
  args = str.substr(pos);
}

String String::fromWide(wchar_t* ptr)
{
  int count = WideCharToMultiByte(CP_UTF8, 0, ptr, -1, NULL, 0, NULL, NULL);
  char* buf = _new(count);
  WideCharToMultiByte(CP_UTF8, 0, ptr, -1, buf, count, NULL, NULL);
  buf[count - 1] = 0;
  _len(buf) = count - 1;
  _ref(buf) = 1;
  return String::String((void*) buf);
}
wchar_t* String::toWide() const
{
  int count = MultiByteToWideChar(CP_UTF8, 0, buf, _len(buf) + 1, NULL, 0);
  wchar_t* wide = new wchar_t[count];
  MultiByteToWideChar(CP_UTF8, 0, buf, _len(buf) + 1, wide, count);
  return wide;
}
wchar_t* String::toWide(int& size) const
{
  size = MultiByteToWideChar(CP_UTF8, 0, buf, _len(buf) + 1, NULL, 0);
  wchar_t* wide = new wchar_t[size];
  MultiByteToWideChar(CP_UTF8, 0, buf, _len(buf) + 1, wide, size);
  return wide;
}
void String::toWide(wchar_t* ptr, int length) const
{
  MultiByteToWideChar(CP_UTF8, 0, buf, _len(buf) + 1, ptr, length);
}

String& String::toAnsi()
{
  if (_ref(buf) > 1) splice();

  int count = MultiByteToWideChar(CP_UTF8, 0, buf, _len(buf), NULL, 0);
  wchar_t* wide = new wchar_t[count];
  MultiByteToWideChar(CP_UTF8, 0, buf, _len(buf), wide, count);

  int ncount = WideCharToMultiByte(CP_ACP, 0, wide, count, NULL, 0, NULL, NULL);
  realloc(ncount);
  WideCharToMultiByte(CP_ACP, 0, wide, count, buf, ncount, NULL, NULL);
  _len(buf) = ncount;
  buf[ncount] = 0;
  delete[] wide;

  return *this;
}
int String::getUtfLength() const
{
  int length = 0;
  for (uint8_const_ptr ptr = (uint8_const_ptr) buf; *ptr; ptr = utf8::next(ptr))
    length++;
  return length;
}
int String::fromUtfPos(int pos) const
{
  uint8_const_ptr ptr = (uint8_const_ptr) buf;
  while (*ptr && pos)
  {
    ptr = utf8::next(ptr);
    pos--;
  }
  return ptr - (uint8_const_ptr) buf;
}
int String::toUtfPos(int pos) const
{
  uint8_const_ptr ptr = (uint8_const_ptr) buf;
  uint8_const_ptr end = (uint8_const_ptr) (buf + pos);
  int count = 0;
  while (ptr < end)
  {
    ptr = utf8::next(ptr);
    count++;
  }
  if (ptr > end)
    count--;
  return count;
}

bool String::toClipboard() const
{
  int count = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, buf, -1, NULL, 0);
  wchar_t* wide = new wchar_t[count + 5];
  MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, buf, -1, wide, count + 5);
  if (!OpenClipboard(NULL))
  {
    delete[] wide;
    return false;
  }
  EmptyClipboard();
  int sz = wcslen(wide);
  HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, sz * 2 + 2);
  if (!handle)
  {
    CloseClipboard();
    delete[] wide;
    return false;
  }
  wchar_t* copy = (wchar_t*) GlobalLock(handle);
  memcpy(copy, wide, sz * 2 + 2);
  GlobalUnlock(handle);
  SetClipboardData(CF_UNICODETEXT, (HANDLE) handle);
  CloseClipboard();
  delete[] wide;
  return true;
}
bool String::fromClipboard()
{
  if (!IsClipboardFormatAvailable(CF_UNICODETEXT))
    return false;
  if (!OpenClipboard(NULL))
    return false;
  wchar_t* wide = (wchar_t*) GetClipboardData(CF_UNICODETEXT);
  CloseClipboard();
  if (wide)
  {
    if (_ref(buf) > 1) splice();
    int count = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, (LPWSTR) wide, -1, NULL, 0, NULL, NULL);
    realloc(count);
    WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, (LPWSTR) wide, -1, buf, _size(buf), NULL, NULL);
    int len = 0;
    for (int i = 0; buf[i]; i++)
      if (buf[i] != '\r')
        buf[len++] = buf[i];
    buf[len] = 0;
    _len(buf) = len;
    return true;
  }
  else
    return false;
}

#include "regexp.h"

bool String::match(char const* re, Array<String>* sub) const
{
  return re::Prog(re).match(buf, sub);
}
int String::rfind(char const* re, int start, Array<String>* sub) const
{
  return re::Prog(re).find(buf, start, sub);
}
String& String::rreplace(char const* re, char const* with)
{
  return (*this) = re::Prog(re).replace(buf, with);
}
int String::split(Array<String>& res, char sep, int flags)
{
  res.clear();
  int prev = 0;
  bool quote = false;
  for (int i = 0; i <= length(); i++)
  {
    if ((buf[i] == sep && !quote) || buf[i] == 0)
    {
      if ((flags & SPLIT_NOEMPTY) == 0 || i > prev)
      {
        String sub = substring(prev, i);
        if (flags & SPLIT_QUOTES)
          sub.dequote();
        res.push(sub);
      }
      prev = i + 1;
    }
    else if (buf[i] == '"')
    {
      if (!quote && (flags & SPLIT_QUOTES) && i == prev)
        quote = true;
      else
        quote = false;
    }
  }
  return res.length();
}
int String::split(Array<String>& res, char const* seplist, int flags)
{
  static bool issep[256];
  memset(issep, 0, sizeof issep);
  for (int i = 0; seplist[i]; i++)
    issep[(uint8) seplist[i]] = true;
  res.clear();
  int prev = 0;
  bool quote = false;
  for (int i = 0; i <= length(); i++)
  {
    if ((issep[(uint8) buf[i]] && !quote) || buf[i] == 0)
    {
      if ((flags & SPLIT_NOEMPTY) == 0 || i > prev)
      {
        String sub = substring(prev, i);
        if (flags & SPLIT_QUOTES)
          sub.dequote();
        res.push(sub);
      }
      prev = i + 1;
    }
    else if (buf[i] == '"')
    {
      if (!quote && (flags & SPLIT_QUOTES) && i == prev)
        quote = true;
      else
        quote = false;
    }
  }
  return res.length();
}

int String::smartCompare(String a, String b)
{
  uint8_const_ptr ta = (uint8_const_ptr) a.buf;
  uint8_const_ptr tb = (uint8_const_ptr) b.buf;
  while (*ta || *tb)
  {
    if (*ta >= '0' && *ta <= '9' && *tb >= '0' && *tb <= '9')
    {
      int na = 0;
      while (*ta >= '0' && *ta <= '9')
        na = na * 10 + int(*ta++ - '0');
      int nb = 0;
      while (*tb >= '0' && *tb <= '9')
        nb = nb * 10 + int(*tb++ - '0');
      if (na != nb)
        return na - nb;
    }
    else
    {
      uint32 ca = utf8::transform(&ta, utf8::tf_lower);
      uint32 cb = utf8::transform(&tb, utf8::tf_lower);
      if (ca != cb)
        return int(ca) - int(cb);
    }
  }
  return 0;
}

#ifdef ENABLE_UTF8

int String::icompare(char const* str) const
{
  uint8_const_ptr a = (uint8_const_ptr) buf;
  uint8_const_ptr b = (uint8_const_ptr) str;
  while (*a || *b)
  {
    uint32 wa = utf8::transform(&a, utf8::tf_lower);
    uint32 wb = utf8::transform(&b, utf8::tf_lower);
    if (wa != wb)
      return int(wa) - int(wb);
  }
  return 0;
}

String String::lower() const
{
  char* result = _new(_size(buf));
  int len = 0;
  uint8_const_ptr ptr = (uint8_const_ptr) buf;
  while (*ptr)
  {
    uint32 cp = utf8::transform(&ptr, utf8::tf_lower);
    while (cp)
    {
      result[len++] = char(cp);
      cp >>= 8;

      if (len >= _size(result))
      {
        int newsize = (_size(result) < ALLOC_STEP ? _size(result) * 2 : _size(result) + ALLOC_STEP);
        if (newsize <= len)
          newsize = len + 1;
        char* tmp = _new(newsize);
        memcpy(tmp, result, len);
        _del(result);
        result = tmp;
      }
    }
  }
  result[len] = 0;
  _len(result) = len;
  _ref(result) = 1;
  return String((void*) result);
}
String String::upper() const
{
  char* result = _new(_size(buf));
  int len = 0;
  uint8_const_ptr ptr = (uint8_const_ptr) buf;
  while (*ptr)
  {
    uint32 cp = utf8::transform(&ptr, utf8::tf_upper);
    while (cp)
    {
      result[len++] = char(cp);
      cp >>= 8;

      if (len >= _size(result))
      {
        int newsize = (_size(result) < ALLOC_STEP ? _size(result) * 2 : _size(result) + ALLOC_STEP);
        if (newsize <= len)
          newsize = len + 1;
        char* tmp = _new(newsize);
        memcpy(tmp, result, len);
        _del(result);
        result = tmp;
      }
    }
  }
  result[len] = 0;
  _len(result) = len;
  _ref(result) = 1;
  return String((void*) result);
}

#else

int String::icompare(char const* str) const
{
  return stricmp(buf, str);
}

String String::lower() const
{
  String result(*this);
  result.splice();
  strlwr(result.buf);
  return result;
}

#endif
