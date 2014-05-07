#ifndef __BASE_UTILS_H__
#define __BASE_UTILS_H__

#include <Windows.h>
#include "base/wstring.h"
#include "base/types.h"
#include "base/array.h"

WideString getAppPath(bool quoted = false);

#define TIME_HOURS      1
#define TIME_SECONDS    2
String format_time(long time, int flags = TIME_SECONDS);

bool browseForFolder(String prompt, String& result);

uint32 flip_int(uint32 i);

uint64 sysTime();
String format_systime(uint64 time, char const* fmt);

float frand();

#endif // __BASE_UTILS_H__
