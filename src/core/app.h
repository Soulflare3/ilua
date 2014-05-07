#ifndef __CORE_APP_H__
#define __CORE_APP_H__

#include <windows.h>
#include "base/wstring.h"
#include "core/frameui/framewnd.h"
#include "api/engine.h"
#include "core/ui/mainwnd.h"

class Application
{
  static Application* instance;
  HINSTANCE hInstance;
  friend Application* getApp();
  WideString root;
  api::Engine e;

  MainWnd* mainWindow;
public:
  Application(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow);
  ~Application();

  HINSTANCE getInstanceHandle() const
  {
    return hInstance;
  }
  WideString getRootPath() const
  {
    return root;
  }
  HWND getMainWindow() const;

  int run();
};
inline Application* getApp()
{
  return Application::instance;
}
inline HINSTANCE getInstance()
{
  return getApp()->getInstanceHandle();
}

#endif // __CORE_APP_H__
