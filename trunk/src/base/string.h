#ifndef __BASE_STRING_H__
#define __BASE_STRING_H__

#include <string.h>
#include <ctype.h>
#include "base/array.h"

#define s_isalnum(x)       isalnum((unsigned char) (x))
#define s_isalpha(x)       isalpha((unsigned char) (x))
#define s_iscntrl(x)       iscntrl((unsigned char) (x))
#define s_isdigit(x)       isdigit((unsigned char) (x))
#define s_isgraph(x)       isgraph((unsigned char) (x))
#define s_isleadbyte(x)    isleadbyte((unsigned char) (x))
#define s_islower(x)       islower((unsigned char) (x))
#define s_isprint(x)       isprint((unsigned char) (x))
#define s_ispunct(x)       ispunct((unsigned char) (x))
#define s_isspace(x)       isspace((unsigned char) (x))
#define s_isupper(x)       isupper((unsigned char) (x))
#define s_isxdigit(x)      isxdigit((unsigned char) (x))
#define s_toupper(x)       toupper((unsigned char) (x))
#define s_tolower(x)       tolower((unsigned char) (x))

#define SPLIT_QUOTES        0x01
#define SPLIT_NOEMPTY       0x02

class WideString;

class String
{
protected:
  char* buf;

  static inline char* _new(int size)
  {
    char* val = new char[size + 12];
    ((int*) val)[2] = size;
    return val + 12;
  }
  static inline int& _size(char* buf) {return ((int*) buf)[-1];}
  static inline int& _ref(char* buf) {return ((int*) buf)[-2];}
  static inline int& _len(char* buf) {return ((int*) buf)[-3];}
  static inline void _del(char* buf) {delete (buf - 12);}

  void deref()
  {
    if (buf && --_ref(buf) == 0)
      _del(buf);
  }
  void realloc(int newLen);
  void splice();
  String(void* b)
    : buf((char*) b)
  {}
public:
  String();
  String(const char* str);
  String(const char* str, int length);
  String(String const& str);
  explicit String(WideString const& str);
  explicit String(wchar_t const* str);
  explicit String(wchar_t const* str, int length);
  explicit String(char value);
  explicit String(int value);
  explicit String(float value);
  explicit String(double value);

  String& reserve(int length)
  {
    if (_ref(buf) > 1) splice();
    realloc(length);
    return *this;
  }

  ~String()
  {
    deref();
  }

  bool isNull() const
  {
    return buf == NULL || buf == null.buf;
  }

  int compare(String str) const
  {
    return buf == str.buf ? 0 : strcmp(buf, str.buf);
  }
  int compare(char const* str) const
  {
    return strcmp(buf, str);
  }
  int icompare(String str) const
  {
    return buf == str.buf ? 0 : icompare(str.buf);
  }
  int icompare(char const* str) const;

  operator const char * () const
  {
    return buf;
  }

  const char* c_str() const
  {
    return buf;
  }
  char* getBuffer()
  {
    if (_ref(buf) > 1) splice();
    return buf;
  }
  int getBufferSize()
  {
    return _size(buf);
  }
  String& recalculate()
  {
    _len(buf) = strlen(buf);
    return *this;
  }

  String& append(char const* str, int len = -1);

  String& operator = (String const& str);
  String& operator = (char const* str);

  String& operator += (String str)
  {
    return append(str.buf, _len(str.buf));
  }
  String& operator += (char const* str)
  {
    return append(str, strlen(str));
  }

  friend String operator + (char const* str1, String str2)
  {
    return String(str1) += str2;
  }
  friend String operator * (int n, String str2)
  {
    return String(str2) *= n;
  }

  String& operator += (char ch);
  String& operator *= (int n);

  String operator + (String str) const
  {
    return String(*this).append(str.buf, _len(str.buf));
  }
  String operator + (char const* str) const
  {
    return String(*this).append(str, strlen(str));
  }
  String operator + (char ch) const
  {
    return String(*this) += ch;
  }
  String operator * (int n) const
  {
    return String(*this) *= n;
  }

  bool operator == (String str) const
  {
    return buf == str.buf || strcmp(buf, str.buf) == 0;
  }
  bool operator != (String str) const
  {
    return buf != str.buf && strcmp(buf, str.buf) != 0;
  }
  bool operator > (String str) const
  {
    return buf != str.buf && strcmp(buf, str.buf) > 0;
  }
  bool operator < (String str) const
  {
    return buf != str.buf && strcmp(buf, str.buf) < 0;
  }
  bool operator >= (String str) const
  {
    return buf == str.buf || strcmp(buf, str.buf) >= 0;
  }
  bool operator <= (String str) const
  {
    return buf == str.buf || strcmp(buf, str.buf) <= 0;
  }

  bool operator == (char const* str) const
  {
    return strcmp(buf, str) == 0;
  }
  friend bool operator == (char const* str, String str2)
  {
    return strcmp(str, str2.buf) == 0;
  }

  bool operator != (char const* str) const
  {
    return strcmp(buf, str) != 0;
  }
  friend bool operator != (char const* str, String str2)
  {
    return strcmp(str, str2.buf) != 0;
  }

  char operator [] (int index) const
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

  String& toUpper()
  {
    return *this = upper();
  }
  String upper() const;

  String& toLower()
  {
    return *this = lower();
  }
  String lower() const;

  int toInt() const;
  float toFloat() const;
  double toDouble() const;

  bool isAlpha() const;
  bool isAlNum() const;
  bool isDigits() const;
  bool isHexDigits() const;
  bool isInt() const;
  bool isFloat() const;

  enum {toTheEnd = 0x7FFFFFFF};

  String substr(int from, int len = toTheEnd) const;
  String substring(int from, int to = toTheEnd) const;
  String& resize(int newlen, char c = 0);
  String& cut(int from, int len = toTheEnd);
  String& trim();
  String& dequote();

  String& removeLeadingSpaces();
  String& removeTrailingSpaces();

  bool isWordBoundary(int pos) const;
  int find(char const* str, int start = 0) const;
  int ifind(char const* str, int start = 0) const;
  int find(char ch, int start = 0) const;
  int ifind(char ch, int start = 0) const;
  int indexOf(char ch) const;
  int lastIndexOf(char ch) const;

  String& replace(int pos, char ch);
  String& replace(int start, int len, char const* str);
  String& insert(int pos, char ch);
  String& insert(int pos, char const* str);
  String& remove(int pos)
  {
    return cut(pos, 1);
  }
  String& remove(int pos, int len)
  {
    return cut(pos, len);
  }

  static String format(const char*, ...);
  String& printf(const char*, ...);

  static String getExtension(String fileName);
  static void setExtension(String& fileName, String ext);
  static bool isRelPath(String path);
  static String getPath(String fullName);
  static String getFileName(String fullName);
  static String getFileTitle(String fullName);
  static String buildFullName(String path, String name);

  static bool isWhitespace(unsigned char c);
  static bool isIdChar(unsigned char c);
  static bool isIdStart(unsigned char c);
  static bool isIdentifier(String str);

  static String fromWide(wchar_t* ptr);
  wchar_t* toWide() const;
  wchar_t* toWide(int& size) const;
  void toWide(wchar_t* ptr, int length) const;

  String& toAnsi();
  int getUtfLength() const;
  int toUtfPos(int pos) const;
  int fromUtfPos(int pos) const;

  static void parseString(String str, String& cmd, String& args);

  static String null;

  bool match(char const* re, Array<String>* sub = NULL) const;
  int rfind(char const* re, int start = 0, Array<String>* sub = NULL) const;
  String& rreplace(char const* re, char const* with);

  int split(Array<String>& res, char sep, int flags = 0);
  int split(Array<String>& res, char const* seplist, int flags = 0);

  static int smartCompare(String a, String b);

  bool toClipboard() const;
  bool fromClipboard();
};

#endif // __BASE_STRING_H__
