#ifndef __BASE_FILE_H__
#define __BASE_FILE_H__

#include "base/types.h"
#include "base/string.h"

#ifdef getc
#undef getc
#undef putc
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

class File;
class FileLoader
{
  int refCount;
public:
  FileLoader()
  {
    refCount = 1;
  }
  virtual ~FileLoader() {}

  FileLoader* retain()
  {
    ++refCount;
    return this;
  }
  int release()
  {
    if (--refCount)
      return refCount;
    delete this;
    return 0;
  }

  virtual File* load(char const* name) = 0;

  static FileLoader* system;
};

class File
{
public:
  File()
  {}
  virtual ~File() {}

  virtual bool readonly() const
  {
    return true;
  }

  virtual uint8 getc() = 0;
  virtual uint32 putc(uint8 c)
  {
    return 0;
  }

  virtual uint32 read(void* buf, uint32 count) = 0;
  virtual uint32 write(void const* buf, uint32 count)
  {
    return 0;
  }

  virtual void seek(uint64 pos, uint32 rel) = 0;
  virtual uint64 tell() const = 0;

  virtual bool eof() const = 0;

  virtual bool ok() const {return true;}
  virtual void close() {}

  virtual void flush() {}

  uint16 read16(bool big = false)
  {
    uint16 res = 0;
    read(&res, sizeof res);
    if (big)
      res = (res << 8) | (res >> 8);
    return res;
  }
  uint32 read32(bool big = false)
  {
    uint32 res = 0;
    read(&res, sizeof res);
    if (big)
      res = (res >> 24) | ((res & 0x00FF0000) >> 8) | ((res & 0x0000FF00) << 8) | (res << 24);
    return res;
  }
  uint64 read64(bool big = false)
  {
    uint64 res = 0;
    read(&res, sizeof res);
    if (big)
      res = (res >> 56) |
            ((res & 0x00FF000000000000ULL) >> 40) |
            ((res & 0x0000FF0000000000ULL) >> 24) |
            ((res & 0x000000FF00000000ULL) >> 8) |
            ((res & 0x00000000FF000000ULL) << 8) |
            ((res & 0x0000000000FF0000ULL) << 24) |
            ((res & 0x000000000000FF00ULL) << 40) |
            (res << 56);
    return res;
  }
  uint32 skipString()
  {
    uint32 count = 0;
    while (getc())
      count++;
    return count;
  }
  virtual uint32 readString(char* str)
  {
    uint32 count = 0;
    while (*str++ = getc())
      count++;
    return count;
  }
  String readString()
  {
    String str = "";
    char chr;
    while (chr = getc())
      str += chr;
    return str;
  }
  void write16(uint16 n, bool big = false)
  {
    if (big)
      n = (n << 8) | (n >> 8);
    write(&n, 2);
  }
  void write32(uint32 n, bool big = false)
  {
    if (big)
      n = (n >> 24) | ((n & 0x00FF0000) >> 8) | ((n & 0x0000FF00) << 8) | (n << 24);
    write(&n, 4);
  }
  void write64(uint64 n, bool big = false)
  {
    if (big)
      n = (n >> 56) |
          ((n & 0x00FF000000000000ULL) >> 40) |
          ((n & 0x0000FF0000000000ULL) >> 24) |
          ((n & 0x000000FF00000000ULL) >> 8) |
          ((n & 0x00000000FF000000ULL) << 8) |
          ((n & 0x0000000000FF0000ULL) << 24) |
          ((n & 0x000000000000FF00ULL) << 40) |
          (n << 56);
    write(&n, 8);
  }
  virtual void writeString(char const* str)
  {
    write(str, strlen(str) + 1);
  }

  float readFloat(bool big = false)
  {
    uint32 res = read32(big);
    return *(float*) &res;
  }
  double readDouble(bool big = false)
  {
    uint64 res = read64(big);
    return *(double*) &res;
  }
  void writeFloat(float f, bool big = false)
  {
    write32(*(uint32*) &f, big);
  }
  void writeDouble(double f, bool big = false)
  {
    write64(*(uint64*) &f, big);
  }
  uint32 gets(char* str, uint32 count);

  void printf(char const* fmt, ...);

  enum {READ, REWRITE, MODIFY};

  static File* null;

  static File* open(char const* filename, uint32 mode = READ);
  static File* wopen(wchar_t const* filename, uint32 mode = READ);
  static File* longopen(char const* filename, uint32 mode = READ);
  static bool isValidURL(char const* url);
  static File* openURL(char const* url);
  static File* load(char const* filename, FileLoader* loader = NULL)
  {
    if (loader)
      return loader->load(filename);
    else
      return open(filename);
  }
  static File* memfile(void const* data, uint32 length);
  static void remove(char const* filename);
  static void rename(char const* source, char const* dest);
  static String tempname();
  static File* create(char const* filename);

  uint64 copy(File* file, uint64 count = 0);

  uint32 crc32();
  void md5(void* digest);
};
class TempFile
{
  File* ptr;
public:
  TempFile (File* file)
    : ptr (file)
  {}
  ~TempFile ()
  {
    if (ptr)
      delete ptr;
  }
  operator File* ()
  {
    return ptr;
  }
  File& operator * ()
  {
    return *ptr;
  }
};

class SystemLoader : public FileLoader
{
  String path;
public:
  SystemLoader (String thePath)
  {
    path = thePath;
  }
  File* load (char const* name)
  {
    return File::open (String::buildFullName (path, name));
  }
};

#endif // __BASE_FILE_H__
