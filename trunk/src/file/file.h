#ifndef __IO_SFILE__
#define __IO_SFILE__

#include "ilua/stream.h"
#include "base/types.h"
#include <stdio.h>
#include <Windows.h>

class SystemFile : public ilua::Stream
{
  static const 
  union fint
  {
    struct
    {
      uint32 low;
      uint32 high;
    };
    int64 n;
  };
  HANDLE hFile;
  HANDLE hMap;
  uint32 mode;
  fint realSize;
  fint fileSize;
  fint pos;
  fint viewStart;
  fint viewEnd;
  uint8* view;
  void setview(int64 pos);
  void doresize(int64 newsize);
public:
  enum {OPEN_READ, OPEN_WRITE, OPEN_APPEND};

  SystemFile(HANDLE file, uint32 mode);
  ~SystemFile();

  char getc();
  int putc(char c);

  int read(void* buf, int count);
  int write(void const* buf, int count);

  void seek(int64 pos, int rel);
  int64 tell() const
  {
    return pos.n;
  }

  bool eof() const
  {
    return pos.n >= fileSize.n;
  }
  void close();
  void flush();

  int64 size() const
  {
    return fileSize.n;
  }
  void resize(int64 newsize);
  
  static void bind(lua_State* L);

  int64 copy(ilua::Stream* stream, int64 count = 0);
};

#endif // __IO_SFILE__
