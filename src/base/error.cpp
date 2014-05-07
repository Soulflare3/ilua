#include "error.h"
#include <stdarg.h>
#include <Windows.h>
#include <stdio.h>

bool __error(char const* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  char* buf = new char[_vscprintf(fmt, ap) + 1];
  vsprintf(buf, fmt, ap);
  OutputDebugStringA(buf);
  delete[] buf;
  return true;
}
void trace(char const* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  char* buf = new char[_vscprintf(fmt, ap) + 1];
  vsprintf(buf, fmt, ap);
  OutputDebugStringA(buf);
  delete[] buf;
}
