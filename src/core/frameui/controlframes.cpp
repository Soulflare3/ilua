#include <windows.h>

#include "core/frameui/framewnd.h"
#include "core/frameui/fontsys.h"

#include "controlframes.h"

ButtonFrame::ButtonFrame(WideString text, Frame* parent, int id, int style)
  : WindowFrame(parent)
{
  create(L"Button", text, WS_CHILD | WS_TABSTOP | style, 0);
  setFont(FontSys::getSysFont());
  setId(id);
}
uint32 ButtonFrame::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  if (message == WM_ERASEBKGND)
  {
    if ((GetWindowLong(hWnd, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX)
    {
      HDC hDC = (HDC) wParam;
      HBRUSH hBrush = (HBRUSH) GetClassLong(GetParent(hWnd), GCL_HBRBACKGROUND);
      RECT rc;
      GetClientRect(hWnd, &rc);
      FillRect(hDC, &rc, hBrush);
      InvalidateRect(hWnd, NULL, FALSE);
      return TRUE;
    }
  }
  return M_UNHANDLED;
}

///////////////////////////////////////////////////////

LinkFrame::LinkFrame(WideString text, Frame* parent, int id)
  : WindowFrame(parent)
{
  hFont = NULL;
  uFont = NULL;
  pressed = false;
  hover = false;
  create(text, WS_CHILD, 0);
  setFont(FontSys::getSysFont());
  resetSize();
  setId(id);
}

void LinkFrame::resetSize()
{
  HDC hDC = GetDC(hWnd);
  SelectObject(hDC, hFont);
  SIZE sz;
  WideString text = getText();
  GetTextExtentPoint32(hDC, text, text.length(), &sz);
  ReleaseDC(hWnd, hDC);
  setSize(sz.cx, sz.cy);
}
void LinkFrame::setColor(uint32 color)
{
  _color = color;
  InvalidateRect(hWnd, NULL, TRUE);
}
void LinkFrame::setFlags(int flags)
{
  _flags = flags;
  InvalidateRect(hWnd, NULL, TRUE);
}

uint32 LinkFrame::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  switch (message)
  {
  case WM_SETFONT:
    hFont = (wParam ? (HFONT) wParam : FontSys::getSysFont());
    uFont = FontSys::changeFlags(FontSys::getFlags(hFont) | FONT_UNDERLINE, hFont);
    if (lParam)
      InvalidateRect(hWnd, NULL, TRUE);
    return 0;
  case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hDC = BeginPaint(hWnd, &ps);

      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hWnd, &pt);

      SetBkMode(hDC, TRANSPARENT);
      SetTextColor(hDC, _color);
      if (pressed || hover)
        SelectObject(hDC, uFont);
      else
        SelectObject(hDC, hFont);
      RECT rc = {0, 0, width(), height()};
      WideString text = getText();
      DrawText(hDC, text, text.length(), &rc, _flags);

      EndPaint(hWnd, &ps);
    }
    return 0;
  case WM_MOUSEMOVE:
    {
      int x = LOWORD(lParam);
      int y = HIWORD(lParam);
      if (!hover && x > 0 && y > 0 && x < width() && y < height())
      {
        hover = true;
        TRACKMOUSEEVENT tme;
        memset(&tme, 0, sizeof tme);
        tme.cbSize = sizeof tme;
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
        if (!pressed)
          InvalidateRect(hWnd, NULL, FALSE);
      }
    }
    return 0;
  case WM_MOUSELEAVE:
    {
      hover = false;
      TRACKMOUSEEVENT tme;
      memset(&tme, 0, sizeof tme);
      tme.cbSize = sizeof tme;
      tme.dwFlags = TME_CANCEL | TME_LEAVE;
      tme.hwndTrack = hWnd;
      TrackMouseEvent(&tme);
      if (!pressed)
        InvalidateRect(hWnd, NULL, TRUE);
    }
    return 0;
  case WM_LBUTTONDOWN:
    SetCapture(hWnd);
    pressed = true;
    return 0;
  case WM_LBUTTONUP:
    if (pressed)
    {
      ReleaseCapture();
      pressed = false;
      notify(WM_COMMAND, MAKELONG(id(), BN_CLICKED), (uint32) hWnd);
    }
    return 0;
  }
  return M_UNHANDLED;
}

///////////////////////////////////////////////////////

uint32 EditFrame::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  if (message == WM_CTLCOLOREDIT || message == WM_CTLCOLORSTATIC)
  {
    HDC hDC = (HDC) wParam;
    if ((fgcolor & 0xFF000000) == 0)
      SetTextColor(hDC, fgcolor);
    if ((bgcolor & 0xFF000000) == 0)
      SetBkColor(hDC, bgcolor);
    if (background)
      return (uint32) background;
    return M_UNHANDLED;
  }
  return M_UNHANDLED;
}
EditFrame::EditFrame(Frame* parent, int id, int style)
  : WindowFrame(parent)
{
  bgcolor = 0xFFFFFFFF;
  fgcolor = 0xFFFFFFFF;
  background = NULL;
  create(L"Edit", L"", style | WS_CHILD | WS_TABSTOP, WS_EX_CLIENTEDGE);
  setFont(FontSys::getSysFont());
  setId(id);
}
EditFrame::~EditFrame()
{
  if (background)
    DeleteObject(background);
}
void EditFrame::setFgColor(uint32 color)
{
  fgcolor = color & 0xFFFFFF;
  invalidate();
}
void EditFrame::setBgColor(uint32 color)
{
  bgcolor = color & 0xFFFFFF;
  if (background)
    DeleteObject(background);
  background = CreateSolidBrush(color);
  invalidate();
}

///////////////////////////////////////////////////////

//#define WM_POSTRESIZE           (WM_USER+129)
//uint32 IconEditFrame::onMessage(uint32 message, uint32 wParam, uint32 lParam)
//{
//  if (message == WM_SIZE || message == WM_SETFONT)
//  {
//    uint32 result = Window::onWndMessage(message, wParam, lParam);
//    RECT rc;
//    SendMessage(hWnd, EM_GETRECT, 0, (LPARAM) &rc);
//    rc.left = 16 + 6;
//    SendMessage(hWnd, EM_SETRECT, 0, (LPARAM) &rc);
//    return result;
//  }
//  else if (message == WM_PAINT)
//  {
//    if (icon)
//    {
//      PAINTSTRUCT ps;
//      HDC hDC = BeginPaint(hWnd, &ps);
//      RECT rc;
//      GetClientRect(hWnd, &rc);
//      getApp()->getImageLibrary()->drawAlpha(hDC, icon, 2, rc.bottom / 2 - 8, 16, 16);
//      uint32 result = Window::onWndMessage(WM_PAINT, (WPARAM) hDC, 0);
//      EndPaint(hWnd, &ps);
//      return result;
//    }
//  }
//  else if (message == WM_SETTEXT)
//  {
//    icon = 0;
//    invalidate();
//  }
//  else if (message == WM_CHAR)
//  {
//    if (wParam == VK_RETURN)
//    {
//      PostMessage(GetParent(hWnd), WM_COMMAND, IDOK, (LPARAM) hWnd);
//      return 0;
//    }
//    else
//    {
//      icon = 0;
//      invalidate();
//    }
//  }
//  return M_UNHANDLED;
//}
//IconEditFrame::IconEditFrame(Frame* parent, int id, int style)
//  : WindowFrame(parent)
//{
//  icon = 0;
//  create("Edit", "", style | ES_MULTILINE | WS_CHILD | WS_TABSTOP, WS_EX_CLIENTEDGE);
//  setFont(FontSys::getSysFont());
//  setId(id);
//}
//void IconEditFrame::setIcon(int i)
//{
//  icon = i;
//  invalidate();
//}

///////////////////////////////////////////////////////

void ComboFrame::onMove(uint32 data)
{
  if (hWnd)
  {
    uint32 flags = SWP_NOREPOSITION;
    HWND hWndInsertAfter = NULL;
    if (visible())
    {
      if (IsWindowVisible(hWnd))
        flags |= SWP_NOZORDER;
      else
      {
        flags |= SWP_SHOWWINDOW;
        hWndInsertAfter = HWND_TOP;
      }
    }
    else
      flags |= SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW;
    if (data)
      DeferWindowPos((HDWP) data, hWnd, hWndInsertAfter, left(), top(), width(), boxHeight, flags);
    else
      SetWindowPos(hWnd, hWndInsertAfter, left(), top(), width(), boxHeight, flags);
  }
}
ComboFrame::ComboFrame(Frame* parent, int id, int style)
  : WindowFrame(parent)
{
  boxHeight = 500;
  create(L"ComboBox", L"", style | WS_CHILD | WS_TABSTOP | WS_VSCROLL, 0);
  setFont(FontSys::getSysFont());
  setId(id);
  setHeight(21);
}
void ComboFrame::reset()
{
  SendMessage(hWnd, CB_RESETCONTENT, 0, 0);
}
int ComboFrame::addString(WideString text, int data)
{
  int id = SendMessage(hWnd, CB_ADDSTRING, 0, (uint32) text.c_str());
  if (id != CB_ERR)
    SendMessage(hWnd, CB_SETITEMDATA, id, data);
  return id;
}
void ComboFrame::delString(int pos)
{
  SendMessage(hWnd, CB_DELETESTRING, pos, 0);
}
int ComboFrame::getItemData(int item) const
{
  return SendMessage(hWnd, CB_GETITEMDATA, item, 0);
}
void ComboFrame::setItemData(int item, int data)
{
  SendMessage(hWnd, CB_SETITEMDATA, item, data);
}
int ComboFrame::getCurSel() const
{
  return SendMessage(hWnd, CB_GETCURSEL, 0, 0);
}
void ComboFrame::setCurSel(int sel)
{
  SendMessage(hWnd, CB_SETCURSEL, sel, 0);
}

///////////////////////////////////////////////////////

StaticFrame::StaticFrame(Frame* parent, int id, int style, int exStyle)
  : WindowFrame(parent)
{
  create(L"Static", L"", style | SS_NOTIFY | WS_CHILD | WS_TABSTOP, exStyle);
  setFont(FontSys::getSysFont());
  setId(id);
}
StaticFrame::StaticFrame(WideString text, Frame* parent, int id, int style, int exStyle)
  : WindowFrame(parent)
{
  create(L"Static", text, style | SS_NOTIFY | WS_CHILD | WS_TABSTOP, exStyle);
  setFont(FontSys::getSysFont());
  resetSize();
  setId(id);
}
void StaticFrame::setImage(HANDLE image, int type)
{
  SendMessage(hWnd, STM_SETIMAGE, (WPARAM) type, (LPARAM) image);
}
void StaticFrame::resetSize()
{
  HDC hDC = GetDC(hWnd);
  SelectObject(hDC, getFont());
  SIZE sz;
  WideString text = getText();
  GetTextExtentPoint32(hDC, text, text.length(), &sz);
  ReleaseDC(hWnd, hDC);
  setSize(sz.cx, sz.cy);
}

///////////////////////////////////////////////////////

#include "richedit.h"

struct EditStreamCookie
{
  WideString str;
  int pos;
};
DWORD CALLBACK RichEditFrame::StreamCallback(DWORD_PTR cookie, LPBYTE buff, LONG cb, LONG* pcb)
{
  EditStreamCookie* ck = (EditStreamCookie*) cookie;
  *pcb = ck->str.length() - ck->pos;
  if (*pcb > cb)
    *pcb = cb;
  if (*pcb)
    memcpy(buff, ck->str.c_str() + ck->pos, *pcb);
  ck->pos += *pcb;
  return 0;
}
RichEditFrame::RichEditFrame(Frame* parent, int id, int style)
  : WindowFrame(parent)
{
  create(RICHEDIT_CLASS, L"", style | WS_CHILD | WS_TABSTOP, WS_EX_CLIENTEDGE);
  setFont(FontSys::getSysFont());
  setId(id);
}
void RichEditFrame::setBackgroundColor(uint32 color)
{
  SendMessage(hWnd, EM_SETBKGNDCOLOR, 0, color);
}
void RichEditFrame::setRichText(WideString text)
{
  EditStreamCookie cookie;
  cookie.str = text;
  cookie.pos = 0;
  EDITSTREAM es;
  es.dwCookie = (DWORD_PTR) &cookie;
  es.dwError = 0;
  es.pfnCallback = StreamCallback;
  SendMessage(hWnd, EM_EXLIMITTEXT, 0, (text.length() < 32768 ? 32768 : text.length() + 1));
  SendMessage(hWnd, EM_STREAMIN, SF_RTF, (uint32) &es);
}

/////////////////////////////////

SliderFrame::SliderFrame(Frame* parent, int id, int style)
  : WindowFrame(parent)
{
  create(TRACKBAR_CLASS, L"", style | WS_CHILD | WS_TABSTOP, 0);
  setFont(FontSys::getSysFont());
  setId(id);
}

void SliderFrame::setPos(int pos)
{
  SendMessage(hWnd, TBM_SETPOS, TRUE, pos);
}
void SliderFrame::setRange(int minValue, int maxValue)
{
  SendMessage(hWnd, TBM_SETRANGEMIN, TRUE, minValue);
  SendMessage(hWnd, TBM_SETRANGEMAX, TRUE, maxValue);
}
void SliderFrame::setLineSize(int size)
{
  SendMessage(hWnd, TBM_SETLINESIZE, 0, size);
}
void SliderFrame::setPageSize(int size)
{
  SendMessage(hWnd, TBM_SETPAGESIZE, 0, size);
}
void SliderFrame::setTicFreq(int freq)
{
  SendMessage(hWnd, TBM_SETTICFREQ, freq, 0);
}
int SliderFrame::getPos()
{
  return SendMessage(hWnd, TBM_GETPOS, 0, 0);
}

/////////////////////////////////////////

UpDownFrame::UpDownFrame(Frame* parent, int id, int style)
  : WindowFrame(parent)
{
  create(UPDOWN_CLASS, L"", WS_CHILD | style, 0);
  setId(id);
}

/////////////////////////////////////////

TabFrame::TabFrame(Frame* parent, int id, int style)
  : WindowFrame(parent)
{
  create(WC_TABCONTROL, L"", WS_CHILD | style, 0);
  setFont(FontSys::getSysFont());
  setId(id);
}
Frame* TabFrame::addTab(WideString text, Frame* frame)
{
  if (frame == NULL)
    frame = new Frame(this);
  int pos = tabs.length();
  tabs.push(frame);

  TCITEM item;
  memset(&item, 0, sizeof item);
  item.mask = TCIF_TEXT;
  item.pszText = text.getBuffer();
  TabCtrl_InsertItem(hWnd, pos, &item);
  frame->show(pos == TabCtrl_GetCurSel(hWnd));

  RECT rc;
  GetClientRect(hWnd, &rc);
  int prevWidth = rc.right;
  int prevHeight = rc.bottom;
  TabCtrl_AdjustRect(hWnd, FALSE, &rc);
  frame->setPoint(PT_TOPLEFT, rc.left, rc.top);
  frame->setPoint(PT_BOTTOMRIGHT, rc.right - prevWidth, rc.bottom - prevHeight);

  return frame;
}
void TabFrame::clear()
{
  for (int i = 0; i < tabs.length(); i++)
    tabs[i]->hide();
  tabs.clear();
  TabCtrl_DeleteAllItems(hWnd);
}

uint32 TabFrame::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  if (message == WM_NOTIFY)
  {
    NMHDR* hdr = (NMHDR*) lParam;
    if (hdr->hwndFrom == hWnd && hdr->code == TCN_SELCHANGE)
    {
      int sel = TabCtrl_GetCurSel(hWnd);
      for (int i = 0; i < tabs.length(); i++)
        if (i != sel)
          tabs[i]->hide();
      tabs[sel]->show();
      return 0;
    }
  }
  return M_UNHANDLED;
}
void TabFrame::setCurSel(int sel)
{
  TabCtrl_SetCurSel(hWnd, sel);
  for (int i = 0; i < tabs.length(); i++)
    if (i != sel)
      tabs[i]->hide();
  tabs[sel]->show();
}

////////////////////////////////////

uint32 ColorFrame::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  if (message == WM_PAINT)
  {
    PAINTSTRUCT ps;
    HDC hDC = BeginPaint(hWnd, &ps);
    EndPaint(hWnd, &ps);
    return 0;
  }
  else if (message == WM_ERASEBKGND)
  {
    HDC hDC = (HDC) wParam;
    RECT rc;
    GetClientRect(hWnd, &rc);
    SetBkColor(hDC, color);
    ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
    return TRUE;
  }
  return M_UNHANDLED;
}
ColorFrame::ColorFrame(Frame* parent, uint32 clr)
  : WindowFrame(parent)
{
  color = clr;
  create(L"", WS_CHILD, 0);
}
ColorFrame::~ColorFrame()
{
}

////////////////////////////////////

TreeViewFrame::TreeViewFrame(Frame* parent, int id, int style)
  : WindowFrame(parent)
{
  create(WC_TREEVIEW, L"", style | WS_CHILD | WS_VISIBLE, WS_EX_CLIENTEDGE);
  setId(id);
  setFont(FontSys::getSysFont());
}
void TreeViewFrame::setItemText(HTREEITEM item, WideString text)
{
  TVITEM tvi;
  memset(&tvi, 0, sizeof tvi);
  tvi.hItem = item;
  tvi.mask = TVIF_TEXT;
  tvi.pszText = text.getBuffer();
  TreeView_SetItem(hWnd, &tvi);
}

////////////////////////////////////

DateTimeFrame::DateTimeFrame(Frame* parent, int id, int style)
  : WindowFrame(parent)
{
  create(DATETIMEPICK_CLASS, L"", style | WS_CHILD | WS_VISIBLE, 0);
  setId(id);
  setFont(FontSys::getSysFont());
}
void DateTimeFrame::setFormat(char const* fmt)
{
  DateTime_SetFormat(hWnd, fmt);
}

bool DateTimeFrame::isDateSet() const
{
  SYSTEMTIME st;
  return (DateTime_GetSystemtime(hWnd, &st) == GDT_VALID);
}
uint64 DateTimeFrame::getDate() const
{
  SYSTEMTIME st;
  if (DateTime_GetSystemtime(hWnd, &st) != GDT_VALID)
    return 0;
  FILETIME ft;
  SystemTimeToFileTime(&st, &ft);
  uint64 result = uint64(ft.dwLowDateTime) | (uint64(ft.dwHighDateTime) << 32);
  result = result / 10000000ULL - 11644473600ULL;
  return result;
}
void DateTimeFrame::setNoDate()
{
  DateTime_SetSystemtime(hWnd, GDT_NONE, NULL);
}
void DateTimeFrame::setDate(uint64 date)
{
  date = (date + 11644473600ULL) * 10000000ULL;
  FILETIME ft;
  ft.dwLowDateTime = uint32(date);
  ft.dwHighDateTime = uint32(date >> 32);
  SYSTEMTIME st;
  FileTimeToSystemTime(&ft, &st);
  DateTime_SetSystemtime(hWnd, GDT_VALID, &st);
}
