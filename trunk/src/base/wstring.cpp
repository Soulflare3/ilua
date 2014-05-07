#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <locale>
#include <windows.h>
#include <winuser.h>

#include "wstring.h"

#define BASIC_ALLOC         16
#define ALLOC_STEP          1024

static wchar_t __empty[] = {0, 0,
                            1, 0,
                            1, 0,
                            0};

WideString WideString::null;

WideString::WideString()
{
  buf = __empty + 6;
  _ref(buf)++;
}

WideString::WideString(wchar_t const* str)
{
  if (str)
  {
    int len = wcslen(str);
    buf = _new(len + 1);
    _len(buf) = len;
    _ref(buf) = 1;
    memcpy(buf, str, 2 * (len + 1));
  }
  else
  {
    buf = __empty + 6;
    _ref(buf)++;
  }
}

WideString::WideString(wchar_t const* str, int length)
{
  buf = _new(length + 1);
  _len(buf) = length;
  _ref(buf) = 1;
  memcpy(buf, str, 2 * length);
  buf[length] = 0;
}

WideString::WideString(wchar_t value)
{
  buf = _new(2);
  _len(buf) = 1;
  _ref(buf) = 1;
  buf[0] = value;
  buf[1] = 0;
}

WideString::WideString(WideString const& str)
  : buf(str.buf)
{
  _ref(buf)++;
}

WideString::WideString(int value)
{
  buf = _new(BASIC_ALLOC);
  _ref(buf) = 1;
  swprintf(buf, L"%d", value);
  _len(buf) = wcslen(buf);
}

WideString::WideString(float value)
{
  buf = _new(BASIC_ALLOC);
  _ref(buf) = 1;
  swprintf(buf, L"%f", value);
  _len(buf) = wcslen(buf);
}
WideString::WideString(double value)
{
  buf = _new(BASIC_ALLOC);
  _ref(buf) = 1;
  swprintf(buf, L"%lf", value);
  _len(buf) = wcslen(buf);
}

WideString& WideString::operator = (WideString const& str)
{
  if (&str == this)
    return *this;

  deref();
  buf = str.buf;
  if (buf)
    _ref(buf)++;
  return *this;
}
WideString& WideString::operator = (wchar_t const* str)
{
  if (str == buf)
    return *this;

  if (str)
  {
    if (_ref(buf) > 1) splice();
    int len = wcslen(str);
    realloc(len);
    wcscpy(buf, str);
    _len(buf) = len;
  }
  else
  {
    deref();
    buf = __empty + 6;
    _ref(buf)++;
  }
  return *this;
}

WideString& WideString::operator += (wchar_t ch)
{
  if (_ref(buf) > 1) splice();
  if (_len(buf) + 1 >= _size(buf))
    realloc(_len(buf) + 1);

  buf[_len(buf)++] = ch;
  buf[_len(buf)] = 0;

  return *this;
}

WideString& WideString::operator *= (int n)
{
  if (n == 1)
    return *this;
  if (_ref(buf) > 1) splice();
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
    memcpy(buf + pos + len, buf + pos, len * 2);
  buf[_len(buf) *= n] = 0;

  return *this;
}

void WideString::splice()
{
  wchar_t* tmp = _new(_size(buf));
  _len(tmp) = _len(buf);
  _ref(tmp) = 1;
  memcpy(tmp, buf, 2 * (_len(buf) + 1));
  deref();
  buf = tmp;
}

void WideString::realloc(int newlen)
{
  newlen++;
  if (newlen <= _size(buf))
    return;
  int newsize = (_size(buf) < ALLOC_STEP ? _size(buf) * 2 : _size(buf) + ALLOC_STEP);
  if (newsize < newlen)
    newsize = newlen;
  wchar_t* tmp = _new(newsize);
  _len(tmp) = _len(buf);
  _ref(tmp) = _ref(buf);
  memcpy(tmp, buf, 2 * (_len(buf) + 1));
  _del(buf);
  buf = tmp;
}

WideString& WideString::append(wchar_t const* str, int len)
{
  if (_ref(buf) > 1) splice();
  if (len < 0) len = wcslen(str);
  realloc(_len(buf) + len);
  memcpy(buf + _len(buf), str, 2 * len);
  _len(buf) += len;
  buf[_len(buf)] = 0;
  return *this;
}

WideString WideString::substr(int from, int len) const
{
  if (from < 0) from += _len(buf);
  if (from < 0) from = 0;
  if (from > _len(buf)) from = _len(buf);
  if (len == toTheEnd) len = _len(buf) - from;
  else if (len < 0) len += _len(buf) - from;
  else if (from + len > _len(buf)) len = _len(buf) - from;
  if (len < 0) len = 0;
  wchar_t* str = _new(len + 1);
  _len(str) = len;
  _ref(str) = 1;
  if (len)
    memcpy(str, buf + from, 2 * len);
  str[len] = 0;
  return WideString((void*) str);
}

WideString WideString::substring(int from, int to) const
{
  if (from < 0) from += _len(buf);
  if (from < 0) from = 0;
  if (from > _len(buf)) from = _len(buf);
  if (to < 0) to += _len(buf);
  else if (to == toTheEnd) to = _len(buf);
  else if (to > _len(buf)) to = _len(buf);
  int len = to - from;
  if (len < 0) len = 0;
  wchar_t* str = _new(len + 1);
  _len(str) = len;
  _ref(str) = 1;
  if (len)
    memcpy(str, buf + from, 2 * len);
  str[len] = 0;
  return WideString((void*) str);
}

WideString& WideString::resize(int newlen, wchar_t c)
{
  if (_ref(buf) > 1) splice();
  if (newlen > _len(buf))
  {
    realloc(newlen);
    for (int i = _len(buf); i < newlen; i++)
      buf[i] = c;
  }
  _len(buf) = newlen;
  buf[newlen] = 0;
  return *this;
}

WideString& WideString::removeLeadingSpaces()
{
  int i = 0;
  while (buf[i] && iswspace(buf[i]))
    i++;
  if (i)
  {
    if (_ref(buf) > 1) splice();
    _len(buf) -= i;
    memmove(buf, buf + i, 2 * (_len(buf) + 1));
  }
  return *this;
}

WideString& WideString::removeTrailingSpaces()
{
  int i = _len(buf);
  while (i > 0 && iswspace(buf[i - 1]))
    i--;
  if (i < _len(buf))
  {
    if (_ref(buf) > 1) splice();
    buf[_len(buf) = i] = 0;
  }
  return *this;
}

WideString& WideString::trim()
{
  removeTrailingSpaces();
  removeLeadingSpaces();
  return *this;
}

WideString& WideString::dequote()
{
  if (_len(buf) >= 2 && (
    (buf[0] == '\"' && buf[_len(buf) - 1] == '\"') ||
    (buf[0] == '\'' && buf[_len(buf) - 1] == '\'')))
  {
    if (_ref(buf) > 1) splice();
    memmove(buf, buf + 1, 2 * (_len(buf) -= 2));
    buf[_len(buf)] = 0;
  }
  return *this;
}

WideString& WideString::cut(int from, int len)
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
      memmove(buf + from, buf + from + len, 2 * (_len(buf) - from - len));
    buf[_len(buf) -= len] = 0;
  }
  return *this;
}

int WideString::find(wchar_t const* str, int start) const
{
  static int kmpbuf[256];
  if (start < 0) start += _len(buf);
  if (start < 0) start = 0;
  if (start > _len(buf)) start = _len(buf);
  if (*str == 0) return start;
  int len = wcslen(str);
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
int WideString::ifind(wchar_t const* str, int start) const
{
  static int kmpbuf[256];
  if (start < 0) start += _len(buf);
  if (start < 0) start = 0;
  if (start > _len(buf)) start = _len(buf);
  if (*str == 0) return start;
  int len = wcslen(str);
  int* kmp = kmpbuf;
  if (len > 256)
    kmp = new int[len];
  kmp[0] = 0;
  int result = -1;
  for (int i = 1; i < len; i++)
  {
    kmp[i] = kmp[i - 1];
    while (kmp[i] && towlower(str[kmp[i]]) != towlower(str[i]))
      kmp[i] = kmp[kmp[i] - 1];
    if (towlower(str[kmp[i]]) == towlower(str[i]))
      kmp[i]++;
  }
  int cur = 0;
  for (int i = start; i < _len(buf); i++)
  {
    while (cur && towlower(str[cur]) != towlower(buf[i]))
      cur = kmp[cur - 1];
    if (towlower(str[cur]) == towlower(buf[i]))
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

int WideString::find(wchar_t ch, int start) const
{
  for (int i = start; buf[i]; i++)
    if (buf[i] == ch)
      return i;
  return -1;
}
int WideString::ifind(wchar_t ch, int start) const
{
  ch = towlower(ch);
  for (int i = start; buf[i]; i++)
    if (towlower(buf[i]) == ch)
      return i;
  return -1;
}
int WideString::indexOf(wchar_t ch) const
{
  for (int i = 0; buf[i]; i++)
    if (buf[i] == ch)
      return i;
  return -1;
}
int WideString::lastIndexOf(wchar_t ch) const
{
  for (int i = _len(buf) - 1; i >= 0; i--)
    if (buf[i] == ch)
      return i;
  return -1;
}

WideString& WideString::replace(int pos, wchar_t ch)
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
WideString& WideString::replace(int start, int len, wchar_t const* str)
{
  if (_ref(buf) > 1) splice();
  int strLength = wcslen(str);
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
    memmove(buf + bnew, buf + b, 2 * (_len(buf) - b));
  if (strLength)
    memcpy(buf + start, str, 2 * strLength);
  buf[newlen] = 0;
  _len(buf) = newlen;
  return *this;
}

WideString& WideString::insert(int pos, wchar_t ch)
{
  if (_ref(buf) > 1) splice();
  if (pos < 0) pos += _len(buf);
  if (pos < 0) pos = 0;
  if (pos > _len(buf)) pos = _len(buf);
  realloc(_len(buf) + 1);
  if (pos < _len(buf))
    memmove(buf + pos + 1, buf + pos, 2 * (_len(buf) - pos));
  buf[pos] = ch;
  buf[++_len(buf)] = 0;
  return *this;
}

WideString& WideString::insert(int pos, wchar_t const* str)
{
  if (_ref(buf) > 1) splice();
  if (pos < 0) pos += _len(buf);
  if (pos < 0) pos = 0;
  if (pos > _len(buf)) pos = _len(buf);
  int strLength = wcslen(str);
  realloc(_len(buf) + strLength);
  if (pos < _len(buf))
    memmove(buf + pos + strLength, buf + pos, 2 * (_len(buf) - pos));
  if (strLength)
    memcpy(buf + pos, str, 2 * strLength);
  buf[_len(buf) += strLength] = 0;
  return *this;
}

WideString WideString::format(wchar_t const* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  wchar_t* buf = _new(_vscwprintf(fmt, ap) + 1);
  _len(buf) = _size(buf) - 1;
  _ref(buf) = 1;
  vswprintf(buf, fmt, ap);
  return WideString((void*) buf);
}
WideString& WideString::printf(wchar_t const* fmt, ...)
{
  if (_ref(buf) > 1) splice();

  va_list ap;
  va_start(ap, fmt);

  int len = _vscwprintf(fmt, ap);
  realloc(_len(buf) + len);
  vswprintf(buf + _len(buf), fmt, ap);
  _len(buf) += len;

  return *this;
}

WideString::WideString(String const& str)
{
  int length = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0);
  buf = _new(length + 1);
  _len(buf) = length;
  _ref(buf) = 1;
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), buf, length);
  buf[length] = 0;
}
WideString::WideString(char const* str)
{
  int strlength = strlen(str);
  int length = MultiByteToWideChar(CP_UTF8, 0, str, strlength, NULL, 0);
  buf = _new(length + 1);
  _len(buf) = length;
  _ref(buf) = 1;
  MultiByteToWideChar(CP_UTF8, 0, str, strlength, buf, length);
  buf[length] = 0;
}
WideString::WideString(char const* str, int strlength)
{
  int length = MultiByteToWideChar(CP_UTF8, 0, str, strlength, NULL, 0);
  buf = _new(length + 1);
  _len(buf) = length;
  _ref(buf) = 1;
  MultiByteToWideChar(CP_UTF8, 0, str, strlength, buf, length);
  buf[length] = 0;
}
String::String(WideString const& str)
{
  int length = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0, NULL, NULL);
  buf = _new(length + 1);
  _len(buf) = length;
  _ref(buf) = 1;
  WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.length(), buf, length, NULL, NULL);
  buf[length] = 0;
}
String::String(wchar_t const* str)
{
  int strlength = wcslen(str);
  int length = WideCharToMultiByte(CP_UTF8, 0, str, strlength, NULL, 0, NULL, NULL);
  buf = _new(length + 1);
  _len(buf) = length;
  _ref(buf) = 1;
  WideCharToMultiByte(CP_UTF8, 0, str, strlength, buf, length, NULL, NULL);
  buf[length] = 0;
}
String::String(wchar_t const* str, int strlength)
{
  int length = WideCharToMultiByte(CP_UTF8, 0, str, strlength, NULL, 0, NULL, NULL);
  buf = _new(length + 1);
  _len(buf) = length;
  _ref(buf) = 1;
  WideCharToMultiByte(CP_UTF8, 0, str, strlength, buf, length, NULL, NULL);
  buf[length] = 0;
}

WideString WideString::getExtension(wchar_t const* str)
{
  for (int i = wcslen(str) - 1; i >= 0; i--)
  {
    if (str[i] == '/' || str[i] == '\\')
      return null;
    if (str[i] == '.')
      return WideString(str + i);
  }
  return null;
}
WideString& WideString::setExtension(wchar_t const* ext)
{
  for (int i = _len(buf) - 1; i >= 0; i--)
  {
    if (buf[i] == '/' || buf[i] == '\\')
      return append(ext, wcslen(ext));
    if (buf[i] == '.')
      return replace(i, toTheEnd, ext);
  }
  return append(ext, wcslen(ext));
}
WideString WideString::getPath(wchar_t const* str)
{
  for (int i = wcslen(str) - 1; i >= 0; i--)
    if (str[i] == '/' || str[i] == '\\')
      return WideString(str, i + 1);
  return null;
}
WideString WideString::getFileName(wchar_t const* str)
{
  for (int i = wcslen(str) - 1; i >= 0; i--)
    if (str[i] == '/' || str[i] == '\\')
      return WideString(str + i + 1);
  return WideString(str);
}
WideString WideString::getFileTitle(wchar_t const* str)
{
  int dot = wcslen(str);
  for (int i = dot - 1; i >= 0; i--)
  {
    if (str[i] == '/' || str[i] == '\\')
      return WideString(str + i + 1, dot - i - 1);
    if (str[i] == '.' && str[dot] == 0)
      dot = i;
  }
  return WideString(str, dot);
}
WideString WideString::buildFullName(wchar_t const* path, wchar_t const* name)
{
  int pathlen = wcslen(path);
  if (pathlen == 0)
    return WideString(name);
  int total = pathlen + wcslen(name);
  if (path[pathlen - 1] != '/' && path[pathlen - 1] != '\\')
    total++;
  wchar_t* buf = _new(total + 1);
  _ref(buf) = 1;
  _len(buf) = total;
  memcpy(buf, path, pathlen * 2);
  if (path[pathlen - 1] != '/' && path[pathlen - 1] != '\\')
    buf[pathlen++] = '\\';
  wcscpy(buf + pathlen, name);
  return WideString((void*) buf);
}
WideString WideString::getFullPathName(wchar_t const* path)
{
  if (path[0] == 0)
    path = L".";
  wchar_t wbuf[MAX_PATH];
  int size = GetFullPathName(path, MAX_PATH, wbuf, NULL);
  if (size == 0) wbuf[0] = 0;
  if (size <= MAX_PATH)
    return WideString(wbuf);
  wchar_t* buf = _new(size);
  _ref(buf) = 1;
  _len(buf) = size - 1;
  GetFullPathName(path, size, buf, NULL);
  return WideString((void*) buf);
}
WideString WideString::normcase(wchar_t const* path)
{
  WideString res;
  for (int i = 0; path[i]; i++)
  {
    if (path[i] == '/')
      res += '\\';
    else
      res += (wchar_t) towlower(path[i]);
  }
  return res;
}
WideString WideString::normpath(wchar_t const* path)
{
  WideString res;
  int pos = 0;
  while (true)
  {
    if (path[pos] == 0 || path[pos] == '/' || path[pos] == '\\')
    {
      if (pos == 0 || wcsncmp(path, L".", pos) == 0)
      {
        // do nothing
      }
      else if (wcsncmp(path, L"..", pos) == 0)
      {
        if (res.length() == 2 && iswalpha(res[0]) && res[1] == ':')
        {
          // do nothing
        }
        else
        {
          int len = res.length();
          while (len > 0 && res[len] != '\\')
            len--;
          res.resize(len);
        }
      }
      else if (pos == 2 && iswalpha(path[0]) && path[1] == ':')
      {
        res.resize(0);
        res.append(path, pos);
      }
      else
      {
        if (res.length() > 0)
          res.append(L"\\", 1);
        res.append(path, pos);
      }
      if (path[pos] == 0)
        break;
      path += pos + 1;
      pos = 0;
    }
    else
      pos++;
  }
  if (res.length() == 2 && iswalpha(res[0]) && res[1] == ':')
    res.append(L"\\", 1);
  return res;
}
WideString WideString::getCurrentDir()
{
  wchar_t tmp[MAX_PATH];
  DWORD req = GetCurrentDirectory(MAX_PATH, tmp);
  if (req < MAX_PATH)
    return WideString(tmp);
  wchar_t* buf = _new(req);
  _ref(buf) = 1;
  _len(buf) = req - 1;
  GetCurrentDirectory(req, buf);
  return WideString((void*) buf);
}
WideString WideString::expandvars(wchar_t const* path)
{
  wchar_t tmp[MAX_PATH];
  DWORD req = ExpandEnvironmentStrings(path, tmp, MAX_PATH);
  if (req <= MAX_PATH)
    return WideString(tmp);
  wchar_t* buf = _new(req);
  _ref(buf) = 1;
  _len(buf) = req - 1;
  ExpandEnvironmentStrings(path, buf, req);
  return WideString((void*) buf);
}
WideString WideString::getenv(wchar_t const* name)
{
  wchar_t tmp[MAX_PATH];
  DWORD req = GetEnvironmentVariable(name, tmp, MAX_PATH);
  if (req < MAX_PATH)
    return WideString(tmp);
  wchar_t* buf = _new(req);
  _ref(buf) = 1;
  _len(buf) = req - 1;
  GetEnvironmentVariable(name, buf, req);
  return WideString((void*) buf);
}
WideString WideString::relpath(wchar_t const* path, wchar_t const* start)
{
  WideString abssrc = getFullPathName(start);
  WideString absdst = getFullPathName(path);
  if (absdst.length() < 2 || abssrc.length() < 2 ||
      !iswalpha(absdst[0]) || absdst[1] != ':' ||
      (absdst[2] != 0 && absdst[2] != '/' && absdst[2] != '\\') ||
      (abssrc[2] != 0 && abssrc[2] != '/' && abssrc[2] != '\\') ||
      towlower(abssrc[0]) != towlower(absdst[0]) || abssrc[1] != absdst[1])
    return absdst;
  int match = 0;
  while (match < abssrc.length() && match < absdst.length() &&
         (towlower(abssrc[match]) == towlower(absdst[match]) ||
          ((abssrc[match] == '\\' || abssrc[match] == '/') &&
           (absdst[match] == '\\' || absdst[match] == '/'))))
    match++;
  while (match > 0 && abssrc[match] != '/' && abssrc[match] != '\\')
    match--;
  WideString result;
  for (int i = abssrc.length() - 2; i >= match; i--)
    if (abssrc[i] == '/' || abssrc[i] == '\\')
      result.append(L"..\\", 3);
  result.append(absdst.c_str() + match + 1);
  return result;
}

WideString& WideString::toUpper()
{
  if (_ref(buf) > 1) splice();
  for (int i = 0; i < _len(buf); i++)
    buf[i] = towupper(buf[i]);
  return *this;
}
WideString& WideString::toLower()
{
  if (_ref(buf) > 1) splice();
  for (int i = 0; i < _len(buf); i++)
    buf[i] = towlower(buf[i]);
  return *this;
}
int WideString::toInt() const
{
  int value;
  swscanf(buf, L"%d", &value);
  return value;
}
float WideString::toFloat() const
{
  float value;
  swscanf(buf, L"%f", &value);
  return value;
}
double WideString::toDouble() const
{
  double value;
  swscanf(buf, L"%lf", &value);
  return value;
}
