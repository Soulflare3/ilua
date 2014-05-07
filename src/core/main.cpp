#include "app.h"
#include <windows.h>
#include <stdio.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
  Application app(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
  return app.run();
}
