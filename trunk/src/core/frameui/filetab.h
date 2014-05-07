#ifndef __FRAMEUI_FILETAB__
#define __FRAMEUI_FILETAB__

#include "core/frameui/frame.h"
#include "core/frameui/window.h"
#include "core/frameui/framewnd.h"
#include <CommCtrl.h>

#define FTN_SELECT      (WM_USER+112)
#define FTN_REMOVE      (WM_USER+113)

class FileTabFrame : public Frame
{
protected:
  class TabBar;
  friend class TabBar;
  struct TabInfo
  {
    Frame* frame;
    bool temp;
    WideString origName;
    WideString name;
    String path;
    int width;
  };
  Array<TabInfo> tabs;
  Array<Frame*> pool;
  int activeTab;
  int mouseoverTab;
  int dragging;
  bool mouseoverX;
  bool pressingX;
  bool bottom;
  bool hover;
  bool auxTab;
  HPEN pens[3];
  HBRUSH brushes[2];
  HICON xicons[3];
  HCURSOR arrow;
  TabBar* bar;
  RECT* getTabRect(int i);
  RECT* getTabRectX(int i);
  void paint(HDC hDC);
  void drawTab(HDC hDC, int i);
  void refreshTab(int i);
  int getTab(int x, int y);
  int getTabX(int x);

  FileTabFrame* auxNext;
  FileTabFrame* auxPrev;
public:
  FileTabFrame(Frame* parent, bool bottom = false, bool aux = false, int id = 0);
  ~FileTabFrame();

  int numTabs() const
  {
    return tabs.length();
  }
  Frame* addTab(String path, Frame* frame = NULL, int pos = 0);
  Frame* getTab(int pos) const
  {
    return (pos < 0 || pos >= tabs.length() ? NULL : tabs[pos].frame);
  }
  char const* getTabPath(int pos) const
  {
    return (pos < 0 || pos >= tabs.length() ? NULL : tabs[pos].path.c_str());
  }
  wchar_t const* getTabOrigName(int pos) const
  {
    return (pos < 0 || pos >= tabs.length() ? NULL : tabs[pos].origName.c_str());
  }
  bool isTabTemp(int pos) const
  {
    return (pos < 0 || pos >= tabs.length() ? false : tabs[pos].temp);
  }
  void setTabPath(int pos, char const* path);
  void setTabTitle(int pos, wchar_t const* name);
  void removeTab(int pos);

  bool hasPool() const
  {
    return pool.length() > 0;
  }

  void clear();

  int getCurSel() const
  {
    return activeTab;
  }
  void setCurSel(int sel);

  void addAux(FileTabFrame* frm);
};

#endif // __FRAMEUI_FILETAB__
