#include <windows.h>
#include <shlobj.h>
#include <time.h>

#include "base/string.h"

#include "utils.h"

WideString getAppPath(bool quoted)
{
  wchar_t const* pt = GetCommandLine();
  char esym = ' ';
  int end = 0;
  if (*pt == '"')
  {
    if (!quoted)
      pt++;
    end = 1;
    esym = '"';
  }
  while (pt[end] && pt[end] != esym)
    end++;
  WideString path = WideString(pt, end);
  for (int i = 0; i < path.length(); i++)
    if (path[i] == '/')
      path.replace(i, '\\');
  if (quoted && esym == '"')
    path += '"';
  return WideString::getPath(path);
}

String format_time(long time, int flags)
{
  String result = "";
  if (time < 0)
  {
    result = "-";
    time = -time;
  }
  time /= 1000;
  if (time >= 3600 && (flags & TIME_HOURS))
    result.printf("%d:%02d", time / 3600, (time / 60) % 60);
  else
    result.printf("%d", time / 60);
  if (flags & TIME_SECONDS)
    result.printf(":%02d", time % 60);
  return result;
}

uint32 flip_int(uint32 i)
{
  return ((i >> 24) & 0x000000FF) |
         ((i >>  8) & 0x0000FF00) |
         ((i <<  8) & 0x00FF0000) |
         ((i << 24) & 0xFF000000);
}

uint64 sysTime()
{
  uint64 t;
  _time64((__time64_t*) &t);
  return t;
}
String format_systime(uint64 time, char const* fmt)
{
  tm temp;
  char buf[128];
  _localtime64_s(&temp, (__time64_t*) &time);
  strftime(buf, sizeof buf, fmt, &temp);
  return buf;
}
float frand()
{
  static uint32 randSeed = GetTickCount();
  randSeed = (randSeed * 1664525) + 1013904223;
  uint32 result = (randSeed & 0x007fffff) | 0x3f800000;
  return (*(float*) &result) - 1.0f;
}
