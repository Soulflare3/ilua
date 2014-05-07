#ifndef __INPUT_KEYLIST__
#define __INPUT_KEYLIST__

#include "base/types.h"

extern char const* keynames[256];
extern unsigned char mousekeys[256];
extern unsigned short chartokey[256];

#define VK_WHEELLEFT      0x0A
#define VK_WHEELRIGHT     0x0B
#define VK_WHEELUP        0x0E
#define VK_WHEELDOWN      0x0F

#define mALT    0x100
#define mSHIFT  0x400
#define mCTRL   0x200
#define mWIN    0x800
#define mUP     0x1000
#define mDOWN   0x2000

#endif // __INPUT_KEYLIST__
