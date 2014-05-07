#ifndef __UI_MAINWND__
#define __UI_MAINWND__

#include <windows.h>
#include "core/ui/editor.h"
#include "core/frameui/filetab.h"
#include "core/frameui/controlframes.h"
#include "base/string.h"
#include "base/hashmap.h"
#include "base/dictionary.h"
#include "core/api/engine.h"
#include "core/frameui/listctrl.h"
#include "core/ui/varlist.h"

#define IDC_DEBUGTAB_FIRST        200
#define IDC_DEBUGTAB_LAST         210
#define WM_CHANGEBREAKPOINT       (WM_USER+234)

class MainWnd : public RootWindow
{
  struct WndSettings
  {
    double splitterPosV;
    double splitterPosH;
    uint32 wndShow;
    uint32 wndX;
    uint32 wndY;
    uint32 wndWidth;
    uint32 wndHeight;
  };
  static WndSettings settings;
  Array<WideString> pathList;
  Dictionary<uint32> pathIds;
  HashMap<uint32, uint64> breakpoints;
  void updateBreakpoints();
  FileTabFrame* tabs;
  api::Engine* engine;
  HICON hIcon[3];
  HCURSOR sizeCursor[2];
  int dragPos;
  int dragIndex;

  bool loaded;

  FileTabFrame* auxTabs[2];
  Editor* logWindow;

  Editor* focusWindow;

  String messageLog;

  static bool bpHandler(int reason, int line, char const* path, void* opaque);
  uint32 onMessage(uint32 message, uint32 wParam, uint32 lParam);
  int findTab(uint32 pathId);
  uint32 getPathId(char const* path);

  struct DebugView
  {
    MainWnd* wnd;
    ListFrame* threads;
    ListFrame* stack;
    VarList* locals;
    VarList* globals;
    VarList* watch;
    int curLevel;
  };
  DebugView dbg;
  void onDebugStart();
  void onDebugEnd();
  void onDebugPause();
  void onDebugResume();
  uint32 onDebugMessage(uint32 message, uint32 wParam, uint32 lParam);

  int iconIndex;
  bool trayShown;
  void createTrayIcon();
  void destroyTrayIcon();
  void trayNotify(WideString title, WideString text);
  void traySetIcon(int index);
  WideString runTitle;
  void updateTitle();
public:
  MainWnd(api::Engine* e);
  ~MainWnd();

  void openFile(wchar_t const* path);
  void start(bool shown);

  api::Engine* getEngine() const
  {
    return engine;
  }
  int getDebugLevel() const
  {
    return dbg.curLevel;
  }
};

#endif // __UI_MAINWND__
