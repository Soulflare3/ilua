#include "filetab.h"
#include "core/frameui/framewnd.h"
#include "core/frameui/fontsys.h"
#include <windowsx.h>
#include "resource.h"
#include "core/app.h"
#include "core/frameui/dragdrop.h"
#include "core/ui/editor.h"

#define BAR_HEIGHT      24
#define TAB_HEIGHT      22

class FileTabFrame::TabBar : public WindowFrame
{
  FileTabFrame* t;
  DropTarget* target;
  uint32 onMessage(uint32 message, uint32 wParam, uint32 lParam);
  int toolHitTest(POINT pt, ToolInfo* ti);
public:
  TabBar(FileTabFrame* parent, int id)
    : WindowFrame(parent)
    , t(parent)
    , target(NULL)
  {
    if (WNDCLASSEX* wcx = createclass(L"FileTabWnd"))
      RegisterClassEx(wcx);
    create(L"", WS_CHILD, 0);
    setId(id);

    if (!parent->auxTab)
      target = new DropTarget(this, CF_UNICODETEXT, DROPEFFECT_COPY | DROPEFFECT_MOVE);
    enableTooltips(true);
  }
};

FileTabFrame::FileTabFrame(Frame* parent, bool btm, bool aux, int id)
  : Frame(parent)
  , activeTab(-1)
  , mouseoverTab(-1)
  , mouseoverX(false)
  , pressingX(false)
  , bottom(btm)
  , hover(false)
  , dragging(-1)
  , auxTab(aux)
  , auxNext(NULL)
  , auxPrev(NULL)
{
  pens[0] = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT));
  pens[1] = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT) - 0x101010);
  pens[2] = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
  brushes[0] = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
  brushes[1] = CreateSolidBrush(GetSysColor(COLOR_BTNFACE) - 0x101010);
  xicons[0] = (HICON) LoadImage(getInstance(), MAKEINTRESOURCE(IDI_XNORMAL),
    IMAGE_ICON, 16, 16, 0);
  xicons[1] = (HICON) LoadImage(getInstance(), MAKEINTRESOURCE(IDI_XHOVER),
    IMAGE_ICON, 16, 16, 0);
  xicons[2] = (HICON) LoadImage(getInstance(), MAKEINTRESOURCE(IDI_XPRESSED),
    IMAGE_ICON, 16, 16, 0);
  arrow = LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW));

  bar = new TabBar(this, id);
  if (bottom)
  {
    bar->setPoint(PT_BOTTOMLEFT, 0, 0);
    bar->setPoint(PT_TOPRIGHT, this, PT_BOTTOMRIGHT, 0, -BAR_HEIGHT);
  }
  else
  {
    bar->setPoint(PT_TOPLEFT, 0, 0);
    bar->setPoint(PT_BOTTOMRIGHT, this, PT_TOPRIGHT, 0, BAR_HEIGHT);
  }
}
FileTabFrame::~FileTabFrame()
{
  DeleteObject(pens[0]);
  DeleteObject(pens[1]);
  DeleteObject(pens[2]);
  DeleteObject(brushes[0]);
  DeleteObject(brushes[1]);
  DeleteObject(xicons[0]);
  DeleteObject(xicons[1]);
  DeleteObject(xicons[2]);
  DeleteObject(arrow);
}
Frame* FileTabFrame::addTab(String path, Frame* frame, int pos)
{
  if (pos < 0 || pos >= tabs.length())
    pos = tabs.length();
  if (frame == NULL)
  {
    if (pool.length() > 0)
    {
      frame = pool[pool.length() - 1];
      pool.pop();
    }
    else
      frame = new Frame(this);
  }
  frame->setParent(this);
  TabInfo& ti = tabs.insert(pos);

  ti.temp = false;
  ti.frame = frame;
  if (path.empty())
  {
    char pbuf[MAX_PATH + 10];
    GetTempPathA(MAX_PATH + 10, pbuf);
    ti.temp = true;
    ti.path = String::buildFullName(pbuf, String::format("ilua_temp_%08x.lua", int(frame)));
    int id = 1;
    while (true)
    {
      ti.origName = WideString::format(L"New file (%d)", id);
      bool found = false;
      for (int i = 0; i < tabs.length(); i++)
        if (i != pos && tabs[i].origName == ti.origName)
          found = true;
      if (!found) break;
      id++;
    }
    ti.name = ti.origName;
  }
  else
  {
    ti.path = path;
    ti.name = ti.origName = WideString(String::getFileName(path));
  }
  SIZE sz = FontSys::getTextSize(FontSys::getSysFont(), ti.name);
  ti.width = sz.cx + (auxTab ? 12 : 32);

  setCurSel(pos);

  if (bottom)
  {
    frame->setPoint(PT_TOPLEFT, 0, 0);
    frame->setPoint(PT_BOTTOMRIGHT, bar, PT_TOPRIGHT, 0, 0);
  }
  else
  {
    frame->setPoint(PT_TOPLEFT, bar, PT_BOTTOMLEFT, 0, 0);
    frame->setPoint(PT_BOTTOMRIGHT, 0, 0);
  }

  bar->invalidate();

  return frame;
}
void FileTabFrame::clear()
{
  for (int i = 0; i < tabs.length(); i++)
    delete tabs[i].frame;
  tabs.clear();
  bar->invalidate();
}
void FileTabFrame::removeTab(int pos)
{
  if (pos == dragging)
  {
    ReleaseCapture();
    dragging = -1;
  }
  else if (pos < dragging)
    dragging--;
  Editor* editor = dynamic_cast<Editor*>(tabs[pos].frame);
  if (editor && !editor->tryClose(tabs[pos].path, tabs[pos].origName))
    return;
  else if (editor && !auxTab)
    editor->clear();
  if (activeTab == pos)
  {
    if (tabs.length() == 1)
      setCurSel(-1);
    else if (pos == tabs.length() - 1)
      setCurSel(pos - 1);
    else
      setCurSel(pos + 1);
  }
  if (!auxTab)
  {
    pool.push(tabs[pos].frame);
    notify(FTN_REMOVE, 0, (uint32) tabs[pos].frame);
  }
  tabs.remove(pos);
  if (activeTab >= pos)
    activeTab--;
  bar->invalidate();
}
RECT* FileTabFrame::getTabRect(int i)
{
  static RECT rc;
  if (bottom)
  {
    rc.top = 0;
    rc.bottom = TAB_HEIGHT;
  }
  else
  {
    rc.top = BAR_HEIGHT - TAB_HEIGHT;
    rc.bottom = BAR_HEIGHT;
  }
  rc.left = 0;
  for (int j = 0; j < i; j++)
    rc.left += tabs[j].width;
  rc.right = rc.left + tabs[i].width;
  return &rc;
}
RECT* FileTabFrame::getTabRectX(int i)
{
  RECT* rc = getTabRect(i);
  if (auxTab)
    rc->right = rc->left - 1;
  if (bottom)
  {
    rc->bottom -= 4;
    rc->top = rc->bottom - 13;
  }
  else
  {
    rc->top += 4;
    rc->bottom = rc->top + 13;;
  }
  rc->right -= 6;
  rc->left = rc->right - 13;
  return rc;
}
static bool ptinrect(POINT const& p, RECT const& r)
{
  return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}
int FileTabFrame::getTab(int x, int y)
{
  POINT pt;
  pt.x = x;
  pt.y = y;
  for (int i = 0; i < tabs.length(); i++)
    if (ptinrect(pt, *getTabRect(i)))
      return i;
  return -1;
}
int FileTabFrame::getTabX(int x)
{
  for (int i = 0; i < tabs.length(); i++)
  {
    RECT* rc = getTabRect(i);
    if (x < rc->right)
      return i;
  }
  return tabs.length() - 1;
}

uint32 FileTabFrame::TabBar::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  switch (message)
  {
  case WM_DESTROY:
    delete target;
    target = NULL;
    break;
  case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hDC = BeginPaint(hWnd, &ps);
      t->paint(hDC);
      EndPaint(hWnd, &ps);
    }
    return 0;
  case WM_ERASEBKGND:
    {
      HDC hDC = (HDC) wParam;
      SetBkColor(hDC, 0x593C2B);
      RECT rc;
      GetClientRect(hWnd, &rc);
      ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
    }
    return TRUE;
  case WM_MOUSEMOVE:
    {
      POINT pt;
      pt.x = GET_X_LPARAM(lParam);
      pt.y = GET_Y_LPARAM(lParam);
      int old = t->mouseoverTab;
      bool oldX = t->mouseoverX;
      t->mouseoverTab = -1;
      for (int i = 0; i < t->tabs.length(); i++)
      {
        if (ptinrect(pt, *t->getTabRect(i)))
        {
          t->mouseoverX = ptinrect(pt, *t->getTabRectX(i));
          t->mouseoverTab = i;
          break;
        }
      }
      if (old != t->mouseoverTab)
      {
        if (old >= 0) t->refreshTab(old);
        if (t->mouseoverTab >= 0) t->refreshTab(t->mouseoverTab);
      }
      else if (oldX != t->mouseoverX)
        t->refreshTab(t->mouseoverTab);

      if (!t->hover)
      {
        t->hover = true;
        TRACKMOUSEEVENT tme;
        memset(&tme, 0, sizeof tme);
        tme.cbSize = sizeof tme;
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
      }
    }
    if (t->auxTab)
    {
      POINT pt;
      pt.x = GET_X_LPARAM(lParam);
      pt.y = GET_Y_LPARAM(lParam);
      ClientToScreen(hWnd, &pt);
      RECT rc;
      GetWindowRect(hWnd, &rc);
      if (!ptinrect(pt, rc))
      {
        FileTabFrame* frm = t;
        while (frm && frm->auxPrev)
          frm = frm->auxPrev;
        while (frm)
        {
          GetWindowRect(frm->bar->hWnd, &rc);
          if (frm != t && ptinrect(pt, rc))
          {
            SetCapture(frm->bar->hWnd);
            frm->bar->notify(WM_MOUSEMOVE, wParam, lParam);
            return 0;
          }
          frm = frm->auxNext;
        }
      }
      int pos = t->getTabX(GET_X_LPARAM(lParam));

      FileTabFrame* frm = t;
      while (frm && frm->auxPrev)
        frm = frm->auxPrev;
      while (frm && frm->dragging < 0)
        frm = frm->auxNext;
      if (frm)
      {
        if (frm != t)
        {
          if (pos < 0) pos = 0;
          int orig = frm->dragging;
          frm->dragging = -1;
          String name = frm->getTabPath(orig);
          if (orig >= 0 && orig < frm->tabs.length() && frm->tabs[orig].temp)
            name = "";
          Frame* tab = frm->getTab(orig);
          frm->removeTab(orig);
          t->addTab(name, tab, pos);
        }
        else
        {
          if (pos == t->dragging)
            return 0;
          TabInfo tmp = t->tabs[pos];
          t->tabs[pos] = t->tabs[t->dragging];
          t->tabs[t->dragging] = tmp;
        }
        t->dragging = pos;
        t->activeTab = pos;
        invalidate();
      }
    }
    return 0;
  case WM_MOUSELEAVE:
    if (t->hover)
    {
      t->hover = false;
      TRACKMOUSEEVENT tme;
      memset(&tme, 0, sizeof tme);
      tme.cbSize = sizeof tme;
      tme.dwFlags = TME_CANCEL | TME_LEAVE;
      tme.hwndTrack = hWnd;
      TrackMouseEvent(&tme);
    }
    if (t->mouseoverTab >= 0)
    {
      int old = t->mouseoverTab;
      t->mouseoverTab = -1;
      t->refreshTab(old);
    }
    return 0;
  case WM_LBUTTONDOWN:
    if (t->mouseoverTab >= 0)
    {
      if (t->mouseoverX && !t->auxTab)
      {
        t->pressingX = true;
        t->refreshTab(t->mouseoverTab);
        SetCapture(hWnd);
      }
      else
      {
        t->setCurSel(t->mouseoverTab);
        t->notify(FTN_SELECT, t->mouseoverTab, (uint32) t->tabs[t->mouseoverTab].frame);

        t->dragging = t->mouseoverTab;
        if (!t->auxTab)
        {
          DoDragDropEx(CF_UNICODETEXT, CreateGlobalText(t->tabs[t->mouseoverTab].path),
                DROPEFFECT_MOVE, hWnd, t->arrow);
          t->dragging = -1;
        }
        else
          SetCapture(hWnd);
      }
    }
    return 0;
  case WM_LBUTTONUP:
    if (t->dragging >= 0)
    {
      t->dragging = -1;
      ReleaseCapture();
    }
    if (t->mouseoverTab >= 0 && t->pressingX)
    {
      ReleaseCapture();
      t->pressingX = false;
      t->removeTab(t->mouseoverTab);
    }
    return 0;
  case WM_DRAGOVER:
    if (t->dragging >= 0)
    {
      int pos = t->getTab(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
      if (pos >= 0 && pos != t->dragging)
      {
        TabInfo tmp = t->tabs[pos];
        t->tabs[pos] = t->tabs[t->dragging];
        t->tabs[t->dragging] = tmp;
        t->dragging = pos;
        t->activeTab = pos;
        invalidate();
      }
      return 0;
    }
    else
    {
      int tab = t->getTab(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
      if (tab >= 0)
        t->setCurSel(tab);
    }
    return TRUE;
  case WM_DRAGDROP:
    return DROPEFFECT_NONE;
  }
  return M_UNHANDLED;
}
int FileTabFrame::TabBar::toolHitTest(POINT pt, ToolInfo* ti)
{
  int tab = t->getTab(pt.x, pt.y);
  if (tab >= 0 && t->tabs[tab].path.length() > 0 && !t->tabs[tab].temp)
  {
    ti->rc = *t->getTabRect(tab);
    ti->text = WideString(t->tabs[tab].path.c_str());
  }
  return tab;
}
void FileTabFrame::refreshTab(int i)
{
  InvalidateRect(bar->getHandle(), getTabRect(i), FALSE);
}
void FileTabFrame::drawTab(HDC hDC, int i)
{
  int def = (i == mouseoverTab || i == activeTab ? 0 : 1);
  SelectObject(hDC, pens[bottom ? 2 : def]);
  SelectObject(hDC, brushes[def]);

  RECT* trc = getTabRect(i);
  RoundRect(hDC, trc->left + 1, trc->top, trc->right - 1, trc->bottom, 4, 4);
  trc->left += 6;
  trc->right -= 4;
  if (bottom)
  {
    trc->top += 6;
    trc->bottom -= 3;
  }
  else
  {
    trc->top += 3;
    trc->bottom -= 6;
  }
  DrawText(hDC, tabs[i].name, tabs[i].name.length(), trc, DT_LEFT | DT_VCENTER);

  if ((i == activeTab || i == mouseoverTab) && !auxTab)
  {
    int id = 0;
    if (mouseoverX) id++;
    if (pressingX) id++;
    RECT* xrc = getTabRectX(i);
    DrawIconEx(hDC, xrc->left, xrc->top, xicons[id], 16, 16, 0, NULL, DI_IMAGE | DI_MASK);
  }
}
void FileTabFrame::paint(HDC hDC)
{
  RECT rc;
  GetClientRect(bar->getHandle(), &rc);

  SelectObject(hDC, FontSys::getSysFont());
  SetBkMode(hDC, TRANSPARENT);

  for (int i = 0; i < tabs.length(); i++)
    if (i != activeTab)
      drawTab(hDC, i);

  if (bottom)
  {
    rc.bottom = 4;
    rc.top = -5;
    DrawEdge(hDC, &rc, EDGE_RAISED, BF_BOTTOM);
    SetBkColor(hDC, GetSysColor(COLOR_BTNFACE));
    rc.bottom = 2;
    rc.top = 0;
  }
  else
  {
    rc.top = rc.bottom - 4;
    rc.bottom += 5;
    DrawEdge(hDC, &rc, EDGE_RAISED, BF_TOP);
    SetBkColor(hDC, GetSysColor(COLOR_BTNFACE));
    rc.bottom -= 5;
    rc.top = rc.bottom - 2;
  }

  if (activeTab >= 0 && activeTab < tabs.length())
    drawTab(hDC, activeTab);

  ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
}
void FileTabFrame::setCurSel(int sel)
{
  activeTab = sel;
  for (int i = 0; i < tabs.length(); i++)
    tabs[i].frame->show(i == sel);
  if (sel >= 0 && sel < tabs.length())
  {
    WindowFrame* frm = dynamic_cast<WindowFrame*>(tabs[sel].frame);
    if (frm)
      SetFocus(frm->getHandle());
  }
  bar->invalidate();
}
void FileTabFrame::setTabPath(int pos, char const* path)
{
  tabs[pos].path = path;
  tabs[pos].origName = WideString(String::getFileName(path));
  setTabTitle(pos, tabs[pos].origName);
}
void FileTabFrame::setTabTitle(int pos, wchar_t const* name)
{
  tabs[pos].name = name;
  SIZE sz = FontSys::getTextSize(FontSys::getSysFont(), tabs[pos].name);
  tabs[pos].width = sz.cx + (auxTab ? 12 : 32);
  bar->invalidate();
}

void FileTabFrame::addAux(FileTabFrame* frm)
{
  frm->auxNext = auxNext;
  if (auxNext)
    auxNext->auxPrev = frm;
  auxNext = frm;
  frm->auxPrev = this;
}
