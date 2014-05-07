#include "app.h"
#include <commctrl.h>
#include <afxres.h>

#include "base/utils.h"
#include "base/dictionary.h"
#include "base/args.h"
#include "resource.h"

//#pragma comment(linker,"\"/manifestdependency:type='win32' \
//name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
//processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

Application* Application::instance = NULL;

Application::Application(HINSTANCE _hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
  instance = this;
  mainWindow = NULL;
  hInstance = _hInstance;

  root = WideString::getPath(getAppPath());

  ArgumentParser argParser;
  argParser.registerArgument(L"run", L"true");
  argParser.registerArgument(L"show", L"true");
  ArgumentList args;
  argParser.parse(lpCmdLine, args);

  INITCOMMONCONTROLSEX iccex;
  iccex.dwSize = sizeof iccex;
  iccex.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS |
      ICC_BAR_CLASSES | ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES |
      ICC_TAB_CLASSES | ICC_UPDOWN_CLASS | ICC_DATE_CLASSES;
  InitCommonControlsEx(&iccex);
  LoadLibrary(L"Riched20.dll");
  OleInitialize(NULL);

  mainWindow = new MainWnd(&e);
  e.setHandler(mainWindow->getHandle());

  bool eRun = false;
  if (args.getFreeArgumentCount())
  {
    if (args.hasArgument(L"run"))
      eRun = args.getArgumentBool(L"run");
    else if (WideString::getExtension(args.getFreeArgument(0)).icompare(L"ilua") == 0)
      eRun = true;
  }
  bool wShow = !eRun;
  if (args.hasArgument(L"show"))
    wShow = args.getArgumentBool(L"show");
  mainWindow->start(wShow);
  for (int i = 0; i < args.getFreeArgumentCount(); i++)
    mainWindow->openFile(args.getFreeArgument(i));
  if (args.getFreeArgumentCount() == 0)
    mainWindow->notify(WM_COMMAND, ID_FILE_NEW, 0);
  if (eRun)
    e.load(String(args.getFreeArgument(0)));
}
Application::~Application()
{
  delete mainWindow;

  instance = NULL;
  OleFlushClipboard();
  OleUninitialize();
}

int Application::run()
{
  HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR));
  if (mainWindow)
  {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
      if (!TranslateAccelerator(mainWindow->getHandle(), hAccel, &msg))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

    return msg.wParam;
  }
  else
    return 0;
}

HWND Application::getMainWindow() const
{
  return (mainWindow ? mainWindow->getHandle() : NULL);
}
