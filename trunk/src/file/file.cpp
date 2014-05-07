#include "ilua/ilua.h"
#include "file.h"

const int64 viewSize     = 0x00010000;
const int64 sizeGrow     = 0x00010000;

SystemFile::SystemFile(HANDLE file, uint32 m)
  : hFile(file)
  , hMap(NULL)
  , view(NULL)
  , mode(m)
{
  fileSize.n = 0;
  realSize.n = 0;
  pos.n = 0;
  viewStart.n = 0;
  viewEnd.n = 0;
  if (hFile != INVALID_HANDLE_VALUE)
  {
    fileSize.low = GetFileSize(hFile, &fileSize.high);
    if (fileSize.n > 0)
      hMap = CreateFileMapping(hFile, NULL, mode == OPEN_READ ? PAGE_READONLY : PAGE_READWRITE,
        0, 0, NULL);
    realSize.n = fileSize.n;
    if (mode == OPEN_APPEND)
    {
      pos.n = fileSize.n;
      mode = OPEN_WRITE;
    }
    setview(pos.n);
  }
}
SystemFile::~SystemFile()
{
  close();
}
void SystemFile::close()
{
  if (view) UnmapViewOfFile(view);
  if (hMap) CloseHandle(hMap);
  if (realSize.n > fileSize.n)
  {
    SetFilePointerEx(hFile, *(LARGE_INTEGER*) &fileSize.n, NULL, FILE_BEGIN);
    SetEndOfFile(hFile);
  }
  if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
  view = NULL;
  hMap = NULL;
  hFile = INVALID_HANDLE_VALUE;
  fileSize.n = 0;
  realSize.n = 0;
  pos.n = 0;
  viewStart.n = 0;
  viewEnd.n = 0;
  mode = OPEN_READ;
}
void SystemFile::flush()
{
  if (mode == OPEN_READ) return;
  if (realSize.n > fileSize.n)
  {
    UnmapViewOfFile(view);
    CloseHandle(hMap);
    view = NULL;
    SetFilePointerEx(hFile, *(LARGE_INTEGER*) &fileSize.n, NULL, FILE_BEGIN);
    SetEndOfFile(hFile);
    realSize.n = fileSize.n;
    hMap = CreateFileMapping(hFile, NULL, mode == OPEN_READ ? PAGE_READONLY : PAGE_READWRITE,
      0, 0, NULL);
    setview(viewStart.n);
  }
  else if (view)
    FlushViewOfFile(view, viewEnd.n - viewStart.n);
}

void SystemFile::setview(int64 pos)
{
  if (view) UnmapViewOfFile(view);
  viewStart.n = pos;
  viewEnd.n = (pos + viewSize < realSize.n ? pos + viewSize : realSize.n);
  if (viewEnd.n > viewStart.n)
    view = (uint8*) MapViewOfFile(hMap, mode == OPEN_READ ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS,
      viewStart.high, viewStart.low, viewEnd.n - viewStart.n);
  else
    view = NULL;
}
void SystemFile::doresize(int64 newsize)
{
  if (view) UnmapViewOfFile(view);
  view = NULL;
  if (hMap) CloseHandle(hMap);
  SetFilePointerEx(hFile, *(LARGE_INTEGER*) &newsize, NULL, FILE_BEGIN);
  SetEndOfFile(hFile);
  realSize.n = newsize;
  if (newsize)
  {
    hMap = CreateFileMapping(hFile, NULL, mode == OPEN_READ ? PAGE_READONLY : PAGE_READWRITE,
      0, 0, NULL);
    setview(viewStart.n);
  }
  else
    hMap = NULL;
}
char SystemFile::getc()
{
  if (pos.n >= fileSize.n)
    return 0;
  if (pos.n < viewStart.n || pos.n >= viewEnd.n)
    setview(pos.n);
  return view[(pos.n++) - viewStart.n];
}
int SystemFile::putc(char c)
{
  if (mode == OPEN_READ) return 0;
  if (pos.n >= realSize.n)
    doresize(realSize.n + sizeGrow);
  if (pos.n < viewStart.n || pos.n >= viewEnd.n)
    setview(pos.n);
  view[(pos.n++) - viewStart.n] = c;
  if (pos.n > fileSize.n)
    fileSize.n = pos.n;
  return 1;
}
int SystemFile::read(void* vbuf, int count)
{
  if (pos.n + count > fileSize.n)
    count = fileSize.n - pos.n;
  if (count == 0) return 0;
  uint8* buf = (uint8*) vbuf;
  int total = 0;
  if (pos.n < viewStart.n || pos.n >= viewEnd.n)
    setview(pos.n);
  while (pos.n + count > viewEnd.n)
  {
    int add = viewEnd.n - pos.n;
    memcpy(buf, view + (pos.n - viewStart.n), add);
    total += add;
    count -= add;
    buf += add;
    pos.n = viewEnd.n;
    setview(pos.n);
  }
  if (count)
  {
    memcpy(buf, view + (pos.n - viewStart.n), count);
    total += count;
    pos.n += count;
  }
  return total;
}
int SystemFile::write(void const* vbuf, int count)
{
  if (mode == OPEN_READ || count == 0) return 0;
  uint8 const* buf = (uint8*) vbuf;
  if (pos.n + count > realSize.n)
    doresize(pos.n + count > realSize.n + sizeGrow ? pos.n + count : realSize.n + sizeGrow);
  int total = 0;
  if (pos.n < viewStart.n || pos.n >= viewEnd.n)
    setview(pos.n);
  while (pos.n + count > viewEnd.n)
  {
    int add = viewEnd.n - pos.n;
    memcpy(view + (pos.n - viewStart.n), buf, add);
    total += add;
    count -= add;
    buf += add;
    pos.n = viewEnd.n;
    setview(pos.n);
  }
  if (count)
  {
    memcpy(view + (pos.n - viewStart.n), buf, count);
    total += count;
    pos.n += count;
  }
  if (pos.n > fileSize.n) fileSize.n = pos.n;
  return total;
}
void SystemFile::seek(int64 offset, int rel)
{
  if (rel == SEEK_CUR) offset += pos.n;
  else if (rel == SEEK_END) offset += fileSize.n;
  if (offset < 0) offset = 0;
  if (offset > fileSize.n) offset = fileSize.n;
  pos.n = offset;
}
void SystemFile::resize(int64 newsize)
{
  if (mode == OPEN_READ) return;
  doresize(newsize);
  fileSize.n = newsize;
  if (pos.n > fileSize.n)
    pos.n = fileSize.n;
}
int64 SystemFile::copy(ilua::Stream* stream, int64 count)
{
  if (mode == OPEN_READ || count == 0) return 0;
  if (count == 0)
    count = stream->size() - stream->tell();
  if (pos.n + count > realSize.n)
    doresize(pos.n + count > realSize.n + sizeGrow ? pos.n + count : realSize.n + sizeGrow);
  int64 total = 0;
  if (pos.n < viewStart.n || pos.n >= viewEnd.n)
    setview(pos.n);
  while (count)
  {
    int add = viewEnd.n - pos.n;
    if (add > count) add = count;
    add = stream->read(view + (pos.n - viewStart.n), add);
    total += add;
    count -= add;
    pos.n += add;
    if (count)
      setview(pos.n);
  }
  if (pos.n > fileSize.n) fileSize.n = pos.n;
  return total;
}

static int file_close(lua_State* L)
{
  SystemFile* file = ilua::checkobject<SystemFile>(L, 1, "file");
  file->close();
  return 0;
}

void SystemFile::bind(lua_State* L)
{
  ilua::newtype<SystemFile>(L, "file", "stream");
  ilua::bindmethod(L, "close", file_close);
  lua_pop(L, 2);
}
