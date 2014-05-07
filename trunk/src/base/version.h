#ifndef __BASE_VERSION__
#define __BASE_VERSION__

#include "base/types.h"
#include "base/string.h"

uint32 makeVersion(int major, int minor, int build = 0);
String formatVersion(uint32 version);
uint32 parseVersion(String version);
int vGetMajor(uint32 version);
int vGetMinor(uint32 version);
int vGetBuild(uint32 version);

#endif // __BASE_VERSION__
