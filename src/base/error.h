#ifndef __BASE_ERROR__
#define __BASE_ERROR__

void trace(char const* fmt, ...);
bool __error(char const* fmt, ...);

#ifdef _DEBUG
#define assert(x) \
  do \
  { \
    if (!(x)) \
    { \
    if (__error("%s#%d: Assertion failed: %s", __FILE__, __LINE__, #x)) \
        __debugbreak(); \
    } \
  } while (0)
#else
#define assert(x) \
  do {(void) sizeof(x);} while (0)
#endif

#define error(x)      __error("%s#%d: error: %s", __FILE__, __LINE__, x)

#endif // __BASE_ERROR__
