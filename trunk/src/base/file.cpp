#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <Windows.h>

#include "file.h"
#include "base/checksum.h"

class NullFile : public File
{
  uint64 pos;
public:
  NullFile()
  {
    pos = 0;
  }
  bool readonly() const
  {
    return false;
  }
  uint8 getc()
  {
    return 0;
  }
  uint32 putc(uint8 c)
  {
    pos++;
    return 1;
  }
  uint32 read(void* buf, uint32 count)
  {
    return 0;
  }
  uint32 write(void const* buf, uint32 count)
  {
    pos += count;
    return count;
  }
  void seek(uint64 pos, uint32 rel)
  {
    if (rel == SEEK_SET) pos = rel;
    else pos += rel;
  }
  uint64 tell() const
  {
    return pos;
  }
  bool ok() const
  {
    return true;
  }
  bool eof() const
  {
    return true;
  }
} f__null;
File* File::null = &f__null;

class LongSysFile : public File
{
  enum {bufSize = 512};
  int file;
  bool ronly;
public:
  LongSysFile(int _file, bool _ronly)
  {
    file = _file;
    ronly = _ronly;
  }
  ~LongSysFile()
  {
    if (file >= 0)
      _close(file);
  }
  void close()
  {
    if (file >= 0)
      _close(file);
    file = -1;
  }
  bool readonly() const
  {
    return ronly;
  }

  uint8 getc()
  {
    uint8 res;
    return _read(file, &res, 1) ? res : 0;
  }
  uint32 putc(uint8 c)
  {
    if (!ronly)
      return _write(file, &c, 1);
    else
      return 0;
  }

  uint32 read(void* buf, uint32 count)
  {
    return _read(file, buf, count);
  }
  uint32 write(void const* buf, uint32 count)
  {
    if (!ronly)
      return _write(file, buf, count);
    else
      return 0;
  }

  uint32 readString(char* str)
  {
    uint32 count = 0;
    char chr;
    while (_read(file, &chr, 1) && (*str++ = chr))
      count++;
    return count;
  }
  String readString()
  {
    String str = "";
    char chr;
    while (_read(file, &chr, 1) && chr)
      str += chr;
    return str;
  }

  void seek(uint64 pos, uint32 rel)
  {
    _lseeki64(file, pos, rel);
  }
  uint64 tell() const
  {
    return _telli64(file);
  }

  bool ok() const
  {
    return file >= 0;
  }
  bool eof() const
  {
    return _eof(file) != 0;
  }
};
class SysFile : public File
{
  enum {bufSize = 512};
  FILE* file;
  bool ronly;
  uint32 len;
public:
  SysFile(FILE* _file, bool _ronly)
  {
    file = _file;
    ronly = _ronly;
    len = 0;
    if (file)
    {
      fseek(file, 0, SEEK_END);
      len = ftell(file);
      fseek(file, 0, SEEK_SET);
    }
  }
  ~SysFile()
  {
    if (file)
      fclose(file);
  }
  void close()
  {
    if (file)
      fclose(file);
    file = NULL;
  }
  bool readonly() const
  {
    return ronly;
  }

  uint8 getc()
  {
    int res = fgetc(file);
    if (res == EOF) return 0;
    return res;
  }
  uint32 putc(uint8 c)
  {
    if (!ronly)
      return fputc(c, file) == c ? 1 : 0;
    else
      return 0;
  }

  uint32 readString(char* str)
  {
    uint32 count = 0;
    while (*str++ = fgetc(file))
      count++;
    return count;
  }
  String readString()
  {
    String str = "";
    char chr;
    while (chr = fgetc(file))
      str += chr;
    return str;
  }

  uint32 read(void* buf, uint32 count)
  {
    return fread(buf, 1, count, file);
  }
  uint32 write(void const* buf, uint32 count)
  {
    if (!ronly)
      return fwrite(buf, 1, count, file);
    else
      return 0;
  }

  void seek(uint64 pos, uint32 rel)
  {
    fseek(file, pos, rel);
  }
  uint64 tell() const
  {
    return ftell(file);
  }

  bool ok() const
  {
    return file != NULL;
  }
  bool eof() const
  {
    return ftell(file) >= len;
  }
};
class MemFile : public File
{
  uint8 const* ptr;
  uint32 pos;
  uint32 length;
  bool feof;
  bool own;
public:
  MemFile(void const* mem, uint32 len, bool isown = false)
  {
    ptr = (uint8 const*) mem;
    pos = 0;
    length = len;
    feof = false;
    own = isown;
  }
  ~MemFile()
  {
    if (own)
      delete[] ptr;
  }
  void close()
  {
    if (own)
      delete[] ptr;
    ptr = NULL;
  }

  uint8 getc()
  {
    if (feof = (pos >= length))
      return 0;
    return ptr[pos++];
  }

  uint32 read(void* buf, uint32 count)
  {
    if (feof = (pos + count > length))
      count = length - pos;
    if (count)
      memcpy(buf, ptr + pos, count);
    pos += count;
    return count;
  }

  void seek(uint64 _pos, uint32 rel)
  {
    if (rel == SEEK_SET)
      pos = _pos;
    else if (rel == SEEK_CUR)
      pos += _pos;
    else if (rel == SEEK_END)
      pos = length + _pos;
    if (pos < 0) pos = 0;
    if (pos > length) pos = length;
  }
  uint64 tell() const
  {
    return pos;
  }

  bool eof() const
  {
    return pos >= length;
  }
  bool ok() const
  {
    return ptr != NULL;
  }
};

File* File::open(char const* filename, uint32 mode)
{
  FILE* file = NULL;
  if (mode == READ)
    file = fopen (filename, "rb");
  else if (mode == REWRITE)
    file = fopen (filename, "wb");
  else if (mode == MODIFY)
  {
    file = fopen (filename, "r+");
    if (file == NULL)
      file = fopen (filename, "w+");
  }
  if (file)
    return new SysFile (file, mode == READ);
  else
    return NULL;
}
File* File::wopen(wchar_t const* filename, uint32 mode)
{
  FILE* file = NULL;
  if (mode == READ)
    file = _wfopen(filename, L"rb");
  else if (mode == REWRITE)
    file = _wfopen(filename, L"wb");
  else if (mode == MODIFY)
  {
    file = _wfopen(filename, L"r+");
    if (file == NULL)
      file = _wfopen(filename, L"w+");
  }
  if (file)
    return new SysFile(file, mode == READ);
  else
    return NULL;
}
File* File::longopen(char const* filename, uint32 mode)
{
  int file = -1;
  if (mode == READ)
    file = _open (filename, _O_RDONLY | _O_BINARY);
  else if (mode == REWRITE)
    file = _open (filename, _O_CREAT | _O_TRUNC | _O_BINARY | _O_RDWR, _S_IREAD | _S_IWRITE);
  else if (mode == MODIFY)
    file = _open (filename, _O_RDWR | _O_CREAT | _O_BINARY, _S_IREAD | _S_IWRITE);
  if (file >= 0)
    return new LongSysFile (file, mode == READ);
  else
    return NULL;
}

File* File::memfile(void const* data, uint32 length)
{
  return new MemFile(data, length, true);
}

static SystemLoader sysLoader("");
FileLoader* FileLoader::system = &sysLoader;

void File::printf(char const* fmt, ...)
{
  static char buf[1024];

  va_list ap;
  va_start (ap, fmt);

  int len = _vscprintf(fmt, ap);
  char* dst;
  if (len < 1024)
    dst = buf;
  else
    dst = new char[len + 1];
  vsprintf (dst, fmt, ap);
  write (dst, len);
  if (dst != buf)
    delete[] dst;
}
uint32 File::gets(char* str, uint32 count)
{
  uint32 length = 0;
  uint8 c;
  while ((c = getc()) && length < count - 1)
  {
    if (c != '\r')
      str[length++] = c;
    if (c == '\n')
      break;
    if (c == '\r')
    {
      str[length++] = '\n';
      int pos = tell();
      if (getc() != '\n')
        seek(pos, SEEK_SET);
      break;
    }
  }
  str[length] = 0;
  return length;
}

uint64 File::copy(File* file, uint64 count)
{
  static uint8 buf[4096];
  uint64 done = 0;
  while (count == 0 || done < count)
  {
    uint64 to = sizeof buf;
    if (count != 0 && done + to > count)
      to = count - done;
    to = file->read(buf, to);
    if (to == 0)
      break;
    write(buf, to);
    done += to;
  }
  return done;
}
void File::remove(char const* filename)
{
  DeleteFileA(filename);
}
void File::rename(char const* source, char const* dest)
{
  MoveFileA(source, dest);
}
String File::tempname()
{
  return String(tmpnam(NULL));
}

uint32 File::crc32()
{
  uint32 crc = 0xFFFFFFFF;
  uint8 buf[1024];

  uint64 pos = tell();
  seek(0, SEEK_SET);

  while (int count = read(buf, sizeof buf))
    crc = update_crc(crc, buf, count);

  seek(pos, SEEK_SET);

  return ~crc;
}
void File::md5(void* digest)
{
  MD5 md5;
  uint8 buf[1024];

  uint64 pos = tell();
  seek(0, SEEK_SET);

  while (int count = read(buf, sizeof buf))
    md5.process(buf, count);

  seek(pos, SEEK_SET);

  md5.finish(digest);
}

#include <wininet.h>
#pragma comment (lib, "wininet.lib")

File* File::openURL (char const* url)
{
  HINTERNET hInternet = InternetOpenA("Firefox/3.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
  if (!hInternet)
    return NULL;
  HINTERNET hUrl = InternetOpenUrlA(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
  if (!hUrl)
  {
    InternetCloseHandle(hInternet);
    return NULL;
  }
  uint32 length = 0;
  uint32 lenSize = 4;
  uint32 index = 0;
  if (!HttpQueryInfo(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
    &length, &lenSize, &index) || lenSize != 4)
  {
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return NULL;
  }
  uint8* buf = new uint8[length];
  uint32 bytesRead = 0;
  if (!InternetReadFile(hUrl, buf, length, &bytesRead) || bytesRead != length)
  {
    delete[] buf;
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return NULL;
  }
  InternetCloseHandle(hUrl);
  InternetCloseHandle(hInternet);
  return new MemFile(buf, length, true);
}
bool File::isValidURL(char const* url)
{
  return strnicmp(url, "http:", 5) == 0 ||
    strnicmp(url, "https:", 6) == 0 ||
    strnicmp(url, "ftp:", 4) == 0 ||
    strnicmp(url, "gopher:", 7) == 0;
}
File* File::create(char const* filename)
{
  String path(filename);
  for (int i = 0; i <= path.length(); i++)
    if (path[i] == '\\')
      CreateDirectoryA(path.substring(0, i + 1), NULL);
  return File::open(filename, REWRITE);
}
