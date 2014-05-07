#ifndef __BASE_WSTRING_H__
#define __BASE_WSTRING_H__

#include <string.h>
#include <ctype.h>
#include "base/string.h"

class WideString
{
protected:
  wchar_t* buf;

  static inline wchar_t* _new(int size)
  {
    wchar_t* val = new wchar_t[size + 6];
    ((int*) val)[2] = size;
    return val + 6;
  }
  static inline int& _size(wchar_t* buf) {return ((int*) buf)[-1];}
  static inline int& _ref(wchar_t* buf) {return ((int*) buf)[-2];}
  static inline int& _len(wchar_t* buf) {return ((int*) buf)[-3];}
  static inline void _del(wchar_t* buf) {delete (buf - 6);}

  void deref()
  {
    if (buf && --_ref(buf) == 0)
      _del(buf);
  }
  void realloc(int newLen);
  void splice();
  WideString(void* b)
    : buf((wchar_t*) b)
  {}
public:
  WideString();
  WideString(const wchar_t* str);
  WideString(const wchar_t* str, int length);
  WideString(WideString const& str);
  explicit WideString(String const& str);
  explicit WideString(char const* str);
  explicit WideString(char const* str, int length);
  explicit WideString(wchar_t value);
  explicit WideString(int value);
  explicit WideString(float value);
  explicit WideString(double value);

  WideString& reserve(int length)
  {
    if (_ref(buf) > 1) splice();
    realloc(length);
    return *this;
  }

  ~WideString()
  {
    deref();
  }

  bool isNull() const
  {
    return buf == NULL || buf == null.buf;
  }

  int compare(WideString str) const
  {
    return buf == str.buf ? 0 : wcscmp(buf, str.buf);
  }
  int compare(wchar_t const* str) const
  {
    return wcscmp(buf, str);
  }
  int icompare(WideString str) const
  {
    return buf == str.buf ? 0 : wcsicmp(buf, str.buf);
  }
  int icompare(wchar_t const* str) const
  {
    return wcsicmp(buf, str);
  }

  operator const wchar_t * () const
  {
    return buf;
  }

  const wchar_t* c_str() const
  {
    return buf;
  }
  wchar_t* getBuffer()
  {
    if (_ref(buf) > 1) splice();
    return buf;
  }
  int getBufferSize()
  {
    return _size(buf);
  }
  WideString& recalculate()
  {
    _len(buf) = wcslen(buf);
    return *this;
  }

  WideString& append(wchar_t const* str, int len = -1);

  WideString& operator = (WideString const& str);
  WideString& operator = (wchar_t const* str);

  WideString& operator += (WideString str)
  {
    return append(str.buf, _len(str.buf));
  }
  WideString& operator += (wchar_t const* str)
  {
    return append(str, wcslen(str));
  }

  friend WideString operator + (wchar_t const* str1, WideString str2)
  {
    return WideString(str1) += str2;
  }
  friend WideString operator * (int n, WideString str2)
  {
    return WideString(str2) *= n;
  }

  WideString& operator += (wchar_t ch);
  WideString& operator *= (int n);

  WideString operator + (WideString str) const
  {
    return WideString(*this).append(str.buf, _len(str.buf));
  }
  WideString operator + (wchar_t const* str) const
  {
    return WideString(*this).append(str, wcslen(str));
  }
  WideString operator + (wchar_t ch) const
  {
    return WideString(*this) += ch;
  }
  WideString operator * (int n) const
  {
    return WideString(*this) *= n;
  }

  bool operator == (WideString str) const
  {
    return buf == str.buf || wcscmp(buf, str.buf) == 0;
  }
  bool operator != (WideString str) const
  {
    return buf != str.buf && wcscmp(buf, str.buf) != 0;
  }
  bool operator > (WideString str) const
  {
    return buf != str.buf && wcscmp(buf, str.buf) > 0;
  }
  bool operator < (WideString str) const
  {
    return buf != str.buf && wcscmp(buf, str.buf) < 0;
  }
  bool operator >= (WideString str) const
  {
    return buf == str.buf || wcscmp(buf, str.buf) >= 0;
  }
  bool operator <= (WideString str) const
  {
    return buf == str.buf || wcscmp(buf, str.buf) <= 0;
  }

  bool operator == (wchar_t const* str) const
  {
    return wcscmp(buf, str) == 0;
  }
  friend bool operator == (wchar_t const* str, WideString str2)
  {
    return wcscmp(str, str2.buf) == 0;
  }

  bool operator != (wchar_t const* str) const
  {
    return wcscmp(buf, str) != 0;
  }
  friend bool operator != (wchar_t const* str, WideString str2)
  {
    return wcscmp(str, str2.buf) != 0;
  }

  wchar_t operator [] (int index) const
  {
    return buf[index];
  }

  int length() const
  {
    return _len(buf);
  }

  bool empty() const
  {
    return *buf == 0;
  }

  int toInt() const;
  float toFloat() const;
  double toDouble() const;

  WideString& toUpper();
  WideString upper() const
  {
    return WideString(*this).toUpper();
  }

  WideString& toLower();
  WideString lower() const
  {
    return WideString(*this).toLower();
  }

  enum {toTheEnd = 0x7FFFFFFF};

  WideString substr(int from, int len = toTheEnd) const;
  WideString substring(int from, int to = toTheEnd) const;
  WideString& resize(int newlen, wchar_t c = 0);
  WideString& cut(int from, int len = toTheEnd);
  WideString& trim();
  WideString& dequote();

  WideString& removeLeadingSpaces();
  WideString& removeTrailingSpaces();

  int find(wchar_t const* str, int start = 0) const;
  int ifind(wchar_t const* str, int start = 0) const;
  int find(wchar_t ch, int start = 0) const;
  int ifind(wchar_t ch, int start = 0) const;
  int indexOf(wchar_t ch) const;
  int lastIndexOf(wchar_t ch) const;

  WideString& replace(int pos, wchar_t ch);
  WideString& replace(int start, int len, wchar_t const* str);
  WideString& insert(int pos, wchar_t ch);
  WideString& insert(int pos, wchar_t const* str);
  WideString& remove(int pos)
  {
    return cut(pos, 1);
  }
  WideString& remove(int pos, int len)
  {
    return cut(pos, len);
  }

  static WideString format(wchar_t const* fmt, ...);
  WideString& printf(wchar_t const* fmt, ...);

  static WideString getExtension(wchar_t const* fileName);
  WideString& setExtension(wchar_t const* ext);
  static WideString getPath(wchar_t const* fullName);
  static WideString getFileName(wchar_t const* fullName);
  static WideString getFileTitle(wchar_t const* fullName);
  static WideString buildFullName(wchar_t const* path, wchar_t const* name);
  static WideString getFullPathName(wchar_t const* path);
  static WideString normpath(wchar_t const* path);
  static WideString normcase(wchar_t const* path);
  static WideString getCurrentDir();
  static WideString relpath(wchar_t const* path, wchar_t const* start);
  static WideString getenv(wchar_t const* name);
  static WideString expandvars(wchar_t const* path);

  static WideString null;
};

#endif // __BASE_WSTRING_H__
