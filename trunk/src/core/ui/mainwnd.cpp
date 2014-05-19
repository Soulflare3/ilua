#include "mainwnd.h"
#include "core/api/engine.h"
#include "core/frameui/fontsys.h"
#include "resource.h"
#include <afxres.h>
#include "core/registry.h"
#include "core/app.h"
#include "core/ui/aboutdlg.h"

#define MAINWND_NAME    L"iLua Core"

#define IDI_TRAY              1758
#define WM_TRAYNOTIFY         (WM_USER+674)

MainWnd::WndSettings MainWnd::settings = {
  0.65, // splitterPosV
  0.5, // splitterPosH
  SW_SHOW, // wndShow
  CW_USEDEFAULT, // wndX
  0, // wndY
  CW_USEDEFAULT, // wndWidth
  0, // wndHeight
};

MainWnd::MainWnd(api::Engine* e)
  : tabs(NULL)
  , pathIds(DictionaryMap::pathName)
  , engine(e)
  , loaded(false)
  , trayShown(false)
  , dragPos(0)
  , focusWindow(NULL)
  , dragIndex(-1)
  , iconIndex(0)
{
  //cfg.get("editorNormal", EditorSettings::defaultSettings);
  //cfg.get("editorOutput", EditorSettings::logSettings);
  cfg.get("wndSettings", settings);

  hIcon[0] = (HICON) LoadImage(getInstance(), MAKEINTRESOURCE(IDI_MAINWND), IMAGE_ICON, 16, 16, 0);
  hIcon[1] = LoadIcon(getInstance(), MAKEINTRESOURCE(IDI_MAINWNDGREEN));
  hIcon[2] = LoadIcon(getInstance(), MAKEINTRESOURCE(IDI_MAINWNDRED));
  sizeCursor[0] = LoadCursor(getInstance(), MAKEINTRESOURCE(IDC_HSPLIT));
  sizeCursor[1] = LoadCursor(getInstance(), MAKEINTRESOURCE(IDC_VSPLIT));

  if (WNDCLASSEX* wcx = createclass(L"MainWndClass"))
  {
    wcx->hbrBackground = HBRUSH(COLOR_BTNFACE + 1);
    wcx->hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx->hIcon = LoadIcon(getInstance(), MAKEINTRESOURCE(IDI_MAINWND));
    wcx->lpszMenuName = MAKEINTRESOURCE(IDR_MAINMENU);
    RegisterClassEx(wcx);
  }
  create(CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, MAINWND_NAME,
    WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 0);

  tabs = new FileTabFrame(this);
  tabs->setPoint(PT_TOPLEFT, 0, 0);
  tabs->setPointEx(PT_BOTTOMRIGHT, 1, settings.splitterPosV, 0, 0);
  engine->setBreakpointHandler(bpHandler, this);

  auxTabs[0] = new FileTabFrame(this, true, true);
  auxTabs[0]->setPoint(PT_TOPLEFT, tabs, PT_BOTTOMLEFT, 0, 4);
  auxTabs[0]->setPointEx(PT_BOTTOMRIGHT, settings.splitterPosH, 1, 0, 0);
  auxTabs[1] = new FileTabFrame(this, true, true);
  auxTabs[1]->setPoint(PT_TOPLEFT, auxTabs[0], PT_TOPRIGHT, 4, 0);
  auxTabs[1]->setPoint(PT_BOTTOMRIGHT, 0, 0);
  auxTabs[0]->hide();
  auxTabs[1]->hide();
  auxTabs[0]->addAux(auxTabs[1]);
  logWindow = new Editor(this, &EditorSettings::logSettings);
  logWindow->setPoint(PT_TOPLEFT, auxTabs[0], PT_TOPLEFT, 0, 0);
  logWindow->setPoint(PT_BOTTOMRIGHT, 0, 0);

  memset(&dbg, 0, sizeof dbg);
  dbg.wnd = this;
}
void MainWnd::start(bool shown)
{
  WINDOWPLACEMENT pl;
  memset(&pl, 0, sizeof pl);
  pl.length = sizeof pl;
  GetWindowPlacement(hWnd, &pl);
  pl.flags = 0;
  pl.showCmd = (shown ? settings.wndShow : SW_HIDE);
  if (settings.wndX != CW_USEDEFAULT)
  {
    pl.rcNormalPosition.left = settings.wndX;
    pl.rcNormalPosition.top = settings.wndY;
  }
  if (settings.wndWidth != CW_USEDEFAULT)
  {
    pl.rcNormalPosition.right = settings.wndWidth + pl.rcNormalPosition.left;
    pl.rcNormalPosition.bottom = settings.wndHeight + pl.rcNormalPosition.top;
  }
  SetWindowPlacement(hWnd, &pl);

  tabs->setPointEx(PT_BOTTOMRIGHT, 1, settings.splitterPosV, 0, 0);
  auxTabs[0]->setPointEx(PT_BOTTOMRIGHT, settings.splitterPosH, 1, 0, 0);
  if (!shown)
    createTrayIcon();
  loaded = true;
}
MainWnd::~MainWnd()
{
  engine->setBreakpointHandler(NULL, NULL);
  cfg.set("editorNormal", EditorSettings::defaultSettings);
  cfg.set("editorOutput", EditorSettings::logSettings);
  cfg.set("wndSettings", settings);
}
uint32 MainWnd::getPathId(char const* path)
{
  if (!pathIds.has(path))
  {
    uint32 pathId = pathList.push(WideString(path));
    pathIds.set(path, pathId);
    return pathId;
  }
  else
    return pathIds.get(path);
}
void MainWnd::updateBreakpoints()
{
  if (tabs == NULL) return;

  engine->lock();
  breakpoints.clear();
  for (int t = 0; t < tabs->numTabs(); t++)
  {
    char const* path = tabs->getTabPath(t);
    uint32 pathId = getPathId(path);
    Editor* e = (Editor*) tabs->getTab(t);
    for (int y = 0; y < e->getNumLines(); y++)
    {
      if (e->hasBreakpoint(y))
      {
        uint64 val = (1 << pathId);
        if (breakpoints.has(y))
          val |= breakpoints.get(y);
        breakpoints.set(y, val);
      }
    }
  }
  engine->setBreakpoints(breakpoints.enumStart() != 0);

  engine->unlock();
}

#define WM_DOBREAKPOINT     (WM_USER+97)
#define WM_DOLOADERROR      (WM_USER+98)
bool MainWnd::bpHandler(int reason, int line, char const* path, void* opaque)
{
  MainWnd* wnd = (MainWnd*) opaque;
  if (reason == DBG_BREAK)
  {
    if (!wnd->breakpoints.has(line))
      return false;
    if (path == NULL)
      return true;
    if (!wnd->pathIds.has(path))
      return false;
    uint32 pathId = wnd->pathIds.get(path);
    if (wnd->breakpoints.get(line) & (1 << pathId))
    {
      PostMessage(wnd->hWnd, WM_DOBREAKPOINT, pathId, line);
      return true;
    }
    else
      return false;
  }
  else
  {
    if (!wnd->pathIds.has(path))
      return false;
    uint32 pathId = wnd->pathIds.get(path);
    PostMessage(wnd->hWnd, reason == DBG_LOADERROR ? WM_DOLOADERROR : WM_DOBREAKPOINT, pathId, line);
  }
  return true;
}

static int id_to_debug(uint32 id)
{
  switch (id)
  {
  case ID_DEBUG_START:
    return DBG_RUN;
  case ID_DEBUG_STOP:
    return DBG_HALT;
  case ID_DEBUG_STEP:
    return DBG_STEP;
  case ID_DEBUG_STEPIN:
    return DBG_STEPIN;
  case ID_DEBUG_STEPOUT:
    return DBG_STEPOUT;
  }
  return 0;
}
int MainWnd::findTab(uint32 pathId)
{
  int tab = -1;
  for (int i = 0; i < tabs->numTabs(); i++)
  {
    char const* path = tabs->getTabPath(i);
    if (pathIds.has(path) && pathIds.get(path) == pathId)
    {
      if (((Editor*) tabs->getTab(i))->isRunning())
        return i;
      tab = i;
    }
  }
  return tab;
}
void MainWnd::openFile(wchar_t const* tmppath)
{
  WideString path = WideString::getFullPathName(tmppath);
  String upath(path);
  uint32 pathId = getPathId(upath);
  int tab = findTab(pathId);
  if (tab >= 0)
  {
    tabs->setCurSel(tab);
    updateTitle();
    return;
  }

  Editor* editor = NULL;
  if (tabs->getCurSel() >= 0)
    editor = (Editor*) tabs->getTab(tabs->getCurSel());
  if (!editor || !tabs->isTabTemp(tabs->getCurSel()) || editor->getTextLength())
    editor = (Editor*) tabs->addTab(upath,
      tabs->hasPool() ? NULL : new Editor(tabs));
  else
    tabs->setTabPath(tabs->getCurSel(), upath);
  editor->load(path);
  updateTitle();
}
void MainWnd::updateTitle()
{
  if (!engine->running())
  {
    int sel = tabs->getCurSel();
    if (sel < 0)
      setText(MAINWND_NAME);
    else
      setText(WideString::format(L"%s - %s", tabs->getTabOrigName(sel), MAINWND_NAME));
    iconIndex = 0;
  }
  else if (engine->debugState())
  {
    if (runTitle.empty())
      setText(WideString::format(L"%s (Debugging)", MAINWND_NAME));
    else
      setText(WideString::format(L"%s - %s (Debugging)", runTitle, MAINWND_NAME));
    iconIndex = 2;
  }
  else
  {
    if (runTitle.empty())
      setText(WideString::format(L"%s (Running)", MAINWND_NAME));
    else
      setText(WideString::format(L"%s - %s (Running)", runTitle, MAINWND_NAME));
    iconIndex = 1;
  }
  SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM) hIcon[iconIndex]);
  traySetIcon(iconIndex);
}
uint32 MainWnd::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  uint32 cmd = LOWORD(wParam);
  switch (message)
  {
  case WM_CLOSE:
    for (int i = tabs->numTabs() - 1; i >= 0; i--)
    {
      tabs->setCurSel(i);
      tabs->removeTab(i);
    }
    if (tabs->numTabs() == 0)
      DestroyWindow(hWnd);
    return 0;
  case WM_NOTIFY:
    if (wParam >= IDC_DEBUGTAB_FIRST && wParam <= IDC_DEBUGTAB_LAST)
      return onDebugMessage(message, wParam, lParam);
    break;
  case WM_COMMAND:
    if (LOWORD(wParam) >= IDC_DEBUGTAB_FIRST && LOWORD(wParam) <= IDC_DEBUGTAB_LAST)
      return onDebugMessage(message, wParam, lParam);
    switch (cmd)
    {
    case ID_FILE_NEW:
      tabs->addTab("", tabs->hasPool() ? NULL : new Editor(tabs));
      updateTitle();
      break;
    case ID_FILE_OPEN:
      {
        static wchar_t path[512];
        path[0] = 0;
        OPENFILENAME ofn;
        memset(&ofn, 0, sizeof ofn);
        ofn.lStructSize = sizeof ofn;
        ofn.hwndOwner = hWnd;
        ofn.lpstrFilter = L"Lua files (*.lua;*.ilua)\0*.lua;*.ilua\0All Files\0*\0\0";
        ofn.lpstrFile = path;
        ofn.nMaxFile = 512;
        ofn.lpstrDefExt = L"lua";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (!GetOpenFileName(&ofn))
          break;

        openFile(path);
      }
      break;
    case ID_FILE_CLOSE:
      for (int i = tabs->numTabs() - 1; i >= 0; i--)
        if (tabs->getTab(i) == focusWindow)
          tabs->removeTab(i);
      break;
    case ID_FILE_SAVE:
    case ID_FILE_SAVEAS:
    case ID_FILE_SAVEALL:
      for (int i = 0; i < tabs->numTabs(); i++)
      {
        if (cmd == ID_FILE_SAVEALL || tabs->getTab(i) == focusWindow)
        {
          Editor* editor = (Editor*) tabs->getTab(i);
          if (cmd == ID_FILE_SAVEAS || tabs->isTabTemp(i))
          {
            if (cmd == ID_FILE_SAVEALL)
              tabs->setCurSel(i);
            static wchar_t path[512];
            path[0] = 0;
            OPENFILENAME ofn;
            memset(&ofn, 0, sizeof ofn);
            ofn.lStructSize = sizeof ofn;
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"Lua files (*.lua;*.ilua)\0*.lua;*.ilua\0All Files\0*\0\0";
            ofn.lpstrFile = path;
            ofn.nMaxFile = 512;
            ofn.lpstrDefExt = L"lua";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
            if (!GetSaveFileName(&ofn))
              continue;

            WideString fullpath = WideString::getFullPathName(path);
            String upath(fullpath);
            uint32 pathId = getPathId(upath);
            int tab = findTab(pathId);
            if (tab >= 0 && tab != i)
            {
              MessageBox(hWnd, L"File is currently in use and cannot be overwritten.", L"iLua Core", MB_OK | MB_ICONHAND);
              break;
            }

            tabs->setTabPath(i, String(path));
          }
          editor->save(WideString(tabs->getTabPath(i)));
        }
      }
      updateTitle();
      break;
    case ID_FILE_EXIT:
      PostMessage(hWnd, WM_CLOSE, 0, 0);
      break;

    case ID_EDIT_UNDO:
    case ID_EDIT_REDO:
    case ID_EDIT_CUT:
    case ID_EDIT_COPY:
    case ID_EDIT_PASTE:
    case ID_EDIT_SELECTALL:
    case ID_DEBUG_BREAKPOINT:
      if (focusWindow)
        focusWindow->notify(message, wParam, lParam);
      break;

    case ID_DEBUG_START:
    case ID_DEBUG_STEP:
    case ID_DEBUG_STEPIN:
    case ID_DEBUG_STEPOUT:
      if (!engine->running() && cmd != ID_DEBUG_STEPOUT)
      {
        for (int i = 0; i < tabs->numTabs(); i++)
          ((Editor*) tabs->getTab(i))->save(WideString(tabs->getTabPath(i)));
        int sel = tabs->getCurSel();
        if (sel >= 0)
        {
          if (cmd != ID_DEBUG_START)
            engine->debugContinue(id_to_debug(cmd));
          runTitle = tabs->getTabOrigName(sel);
          engine->load(tabs->getTabPath(sel));
        }
      }
      else if (engine->debugState() == DBG_BREAK ||
              (cmd == ID_DEBUG_START && engine->debugState()))
      {
        for (int i = 0; i < tabs->numTabs(); i++)
          ((Editor*) tabs->getTab(i))->setCurrentLine(-1);
        engine->debugContinue(id_to_debug(cmd));
      }
      break;
    case ID_DEBUG_STOP:
      if (engine->running())
      {
        for (int i = 0; i < tabs->numTabs(); i++)
          ((Editor*) tabs->getTab(i))->setCurrentLine(-1);
        engine->debugContinue(DBG_HALT);
      }
      break;
    case ID_DEBUG_BREAK:
      engine->debugContinue(DBG_BREAK);
      break;

    case ID_HELP_ABOUT:
      AboutDialog::run();
      break;
    }
    break;
  case EN_FOCUSED:
    {
      Frame* wnd = (Frame*) wParam;
      if (dynamic_cast<Editor*>(wnd))
        focusWindow = (Editor*) wnd;
    }
    break;
  case WM_SETFOCUS:
    if (focusWindow)
      SetFocus(focusWindow->getHandle());
    else if (tabs->getCurSel() >= 0)
      SetFocus(((Editor*) tabs->getTab(tabs->getCurSel()))->getHandle());
    break;
  case WM_GETMINMAXINFO:
    {
      MINMAXINFO* mmi = (MINMAXINFO*) lParam;
      mmi->ptMinTrackSize.x = 400;
      mmi->ptMinTrackSize.y = 300;
    }
    return 0;
  case WM_SIZE:
  case WM_MOVE:
    if (loaded)
    {
      WINDOWPLACEMENT pl;
      memset(&pl, 0, sizeof pl);
      pl.length = sizeof pl;
      GetWindowPlacement(hWnd, &pl);
      settings.wndShow = pl.showCmd;
      if (pl.showCmd == SW_SHOWNORMAL)
      {
        settings.wndX = pl.rcNormalPosition.left;
        settings.wndY = pl.rcNormalPosition.top;
        settings.wndWidth = pl.rcNormalPosition.right - pl.rcNormalPosition.left;
        settings.wndHeight = pl.rcNormalPosition.bottom - pl.rcNormalPosition.top;
      }
      tabs->setPointEx(PT_BOTTOMRIGHT, 1, settings.splitterPosV, 0, 0);
      auxTabs[0]->setPointEx(PT_BOTTOMRIGHT, settings.splitterPosH, 1, 0, 0);

      if (message == WM_SIZE)
      {
        if (wParam == SIZE_MINIMIZED)
        {
          ShowWindow(hWnd, SW_HIDE);
          createTrayIcon();
        }
        else
          destroyTrayIcon();
      }
    }
    break;
  case WM_OPENFILE:
    {
      LoadedInfo* info = (LoadedInfo*) wParam;
      String path(info->path);
      uint32 pathId = getPathId(path);
      Editor* editor = NULL;
      int tab = findTab(pathId);
      if (tab >= 0)
      {
        editor = (Editor*) tabs->getTab(tab);
        //TODO: check actual file time?
        if (editor->modified())
          editor->load(info->path);
      }
      if (editor == NULL)
      {
        editor = (Editor*) tabs->addTab(path,
          tabs->hasPool() ? NULL : new Editor(tabs));
        editor->load(info->path);
      }
      editor->setRunning(this);
      editor->setBreakLines(info->validLines);
    }
    break;
  case WM_ENGINESTART:
    onDebugStart();
    break;
  case WM_ENGINEHALT:
    onDebugEnd();
    for (int i = 0; i < tabs->numTabs(); i++)
      ((Editor*) tabs->getTab(i))->setRunning(NULL);
    break;
  case FTN_REMOVE:
    {
      Editor* editor = (Editor*) lParam;
      if (editor == focusWindow)
        focusWindow = NULL;
      updateBreakpoints();
    }
  case FTN_SELECT:
    updateTitle();
    break;
  case EN_SETBREAKPOINT:
    updateBreakpoints();
    break;
  case WM_DOBREAKPOINT:
  case WM_DOLOADERROR:
  case WM_CHANGEBREAKPOINT:
    {
      int tab = findTab(wParam);
      if (tab < 0 && wParam < pathList.length())
      {
        LoadedInfo* info = new LoadedInfo;
        info->path = pathList[wParam];
        onMessage(WM_OPENFILE, (uint32) info, (uint32) engine);
        delete info;
      }
      tab = findTab(wParam);
      if (tab < 0)
        break;
      tabs->setCurSel(tab);
      Editor* e = (Editor*) tabs->getTab(tab);
      if (!e->isRunning())
        e->setRunning(this);
      if (message == WM_DOBREAKPOINT || message == WM_CHANGEBREAKPOINT)
      {
        e->setCursor(lParam, 0);
        e->setCurrentLine(lParam);
        if (message == WM_DOBREAKPOINT)
          onDebugPause();
      }
      else
        e->selectLine(lParam);
      SetForegroundWindow(hWnd);
      SetFocus(e->getHandle());
    }
    break;
  case WM_DBGCONTINUE:
    onDebugResume();
    break;
  case WM_ADDMESSAGE:
    {
      LogMessage* msg = (LogMessage*) wParam;
      logWindow->addLogMessage(msg->text, msg->timestamp, lParam);
      free(msg);
    }
    break;
  case EN_MODIFIED:
    {
      Editor* editor = (Editor*) wParam;
      for (int i = 0; i < tabs->numTabs(); i++)
      {
        if (tabs->getTab(i) == editor)
        {
          WideString name(tabs->getTabOrigName(i));
          if (editor->isRunning())
            name += L"~";
          else if (editor->modified())
            name += L"*";
          tabs->setTabTitle(i, name);
        }
      }
    }
    break;
  case WM_TRAYNOTIFY:
    switch (LOWORD(lParam))
    {
    case WM_LBUTTONUP:
      ShowWindow(hWnd, SW_SHOW);
      ShowWindow(hWnd, SW_RESTORE);
      break;
    }
    return 0;
  case WM_ERASEBKGND:
    {
      HDC hDC = (HDC) wParam;
      RECT rc;
      GetClientRect(hWnd, &rc);
      SetBkColor(hDC, 0x593C2B);
      rc.top = rc.bottom - 20;
      ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
      SetBkColor(hDC, GetSysColor(COLOR_BTNFACE));
      rc.bottom = rc.top;
      rc.top = 0;
      ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
      DrawEdge(hDC, &rc, EDGE_RAISED, BF_BOTTOM);
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
//////////////////// SPLITTER ////////////////////////
  case WM_SETCURSOR:
    if (LOWORD(lParam) == HTCLIENT)
    {
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hWnd, &pt);
      if (pt.y > settings.splitterPosV * height() - 2 && pt.y < settings.splitterPosV * height() + 6)
      {
        SetCursor(sizeCursor[0]);
        return TRUE;
      }
      else if (pt.y > settings.splitterPosV * height() &&
               pt.x > settings.splitterPosH * width() - 2&& pt.x < settings.splitterPosH * width() + 6)
      {
        SetCursor(sizeCursor[1]);
        return TRUE;
      }
    }
    return M_UNHANDLED;
  case WM_LBUTTONDOWN:
    {
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hWnd, &pt);
      if (pt.y > settings.splitterPosV * height() - 2 && pt.y < settings.splitterPosV * height() + 6)
      {
        SetCapture(hWnd);
        dragPos = pt.y;
        dragIndex = 0;
        return 0;
      }
      else if (pt.y > settings.splitterPosV * height() &&
               pt.x > settings.splitterPosH * width() - 2&& pt.x < settings.splitterPosH * width() + 6)
      {
        SetCapture(hWnd);
        dragPos = pt.x;
        dragIndex = 1;
        return 0;
      }
    }
    return M_UNHANDLED;
  case WM_MOUSEMOVE:
    if ((wParam & MK_LBUTTON) && GetCapture() == hWnd)
    {
      if (dragIndex == 0)
      {
        int ht = height();
        int pos = short(HIWORD(lParam));
        settings.splitterPosV = (settings.splitterPosV * ht + pos - dragPos) / double(ht);
        dragPos = pos;
        double oldSplitterPos = settings.splitterPosV;
        if (settings.splitterPosV < 0.1) settings.splitterPosV = 0.1;
        if (settings.splitterPosV > 0.9) settings.splitterPosV = 0.9;
        dragPos += int((settings.splitterPosV - oldSplitterPos) * ht);
        tabs->setPointEx(PT_BOTTOMRIGHT, 1, settings.splitterPosV, 0, 0);
      }
      else if (dragIndex == 1)
      {
        int wd = width();
        int pos = short(LOWORD(lParam));
        settings.splitterPosH = (settings.splitterPosH * wd + pos - dragPos) / double(wd);
        dragPos = pos;
        double oldSplitterPos = settings.splitterPosH;
        if (settings.splitterPosH < 0.1) settings.splitterPosH = 0.1;
        if (settings.splitterPosH > 0.9) settings.splitterPosH = 0.9;
        dragPos += int((settings.splitterPosH - oldSplitterPos) * wd);
        auxTabs[0]->setPointEx(PT_BOTTOMRIGHT, settings.splitterPosH, 1, 0, 0);
      }
      return 0;
    }
    return M_UNHANDLED;
  case WM_LBUTTONUP:
    if (GetCapture() == hWnd)
      ReleaseCapture();
    dragIndex = -1;
    break;
  default:
    return M_UNHANDLED;
  }
  return 0;
}

void MainWnd::createTrayIcon()
{
  if (trayShown)
    return;
  NOTIFYICONDATA nid;
  memset(&nid, 0, sizeof nid);
  nid.cbSize = NOTIFYICONDATA_V3_SIZE;
  nid.hWnd = hWnd;
  nid.uID = IDI_TRAY;
  nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
  nid.uCallbackMessage = WM_TRAYNOTIFY;
  nid.hIcon = hIcon[iconIndex];
  wcscpy_s(nid.szTip, sizeof nid.szTip, getText());
  Shell_NotifyIcon(trayShown ? NIM_MODIFY : NIM_ADD, &nid);
  trayShown = true;
}
void MainWnd::destroyTrayIcon()
{
  if (trayShown)
  {
    NOTIFYICONDATA nid;
    memset(&nid, 0, sizeof nid);
    nid.cbSize = NOTIFYICONDATA_V3_SIZE;
    nid.hWnd = hWnd;
    nid.uID = IDI_TRAY;
    Shell_NotifyIcon(NIM_DELETE, &nid);
    trayShown = false;
  }
}
void MainWnd::trayNotify(WideString title, WideString text)
{
  if (trayShown)
  {
    NOTIFYICONDATA nid;
    memset(&nid, 0, sizeof nid);
    nid.cbSize = NOTIFYICONDATA_V3_SIZE;
    nid.hWnd = hWnd;
    nid.uID = IDI_TRAY;

    nid.uFlags = NIF_INFO;
    wcscpy_s(nid.szInfo, sizeof nid.szInfo, text);
    wcscpy_s(nid.szInfoTitle, sizeof nid.szInfoTitle, title);
    nid.uTimeout = 10000;
    nid.dwInfoFlags = NIIF_INFO;

    Shell_NotifyIcon(NIM_MODIFY, &nid);
  }
}
void MainWnd::traySetIcon(int index)
{
  if (trayShown)
  {
    NOTIFYICONDATA nid;
    memset(&nid, 0, sizeof nid);
    nid.cbSize = NOTIFYICONDATA_V3_SIZE;
    nid.hWnd = hWnd;
    nid.uID = IDI_TRAY;

    nid.uFlags = NIF_ICON;
    nid.hIcon = hIcon[index];
    Shell_NotifyIcon(NIM_MODIFY, &nid);
  }
}
