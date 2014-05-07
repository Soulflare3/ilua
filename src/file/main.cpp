#include "file.h"
#include "fileutil.h"

extern "C" __declspec(dllexport) void StartModule(lua_State* L)
{
  SystemFile::bind(L);
  fileutil_bind(L);
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
  return TRUE;
}
