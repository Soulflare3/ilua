#include "editor.h"
#include "core/frameui/fontsys.h"
#include "base/file.h"
#include "core/app.h"
#include "resource.h"
#include "base/utils.h"
#include "mainwnd.h"
#include "luaeval.h"

#include <WindowsX.h>
#include <afxres.h>

void Editor::initkw()
{
  keywords.set(L"and",       1);
  keywords.set(L"break",     1);
  keywords.set(L"do",        1);
  keywords.set(L"else",      1);
  keywords.set(L"elseif",    1);
  keywords.set(L"end",       1);
  keywords.set(L"false",     1);
  keywords.set(L"for",       1);
  keywords.set(L"function",  1);
  keywords.set(L"goto",      1);
  keywords.set(L"if",        1);
  keywords.set(L"in",        1);
  keywords.set(L"local",     1);
  keywords.set(L"nil",       1);
  keywords.set(L"not",       1);
  keywords.set(L"or",        1);
  keywords.set(L"repeat",    1);
  keywords.set(L"return",    1);
  keywords.set(L"then",      1);
  keywords.set(L"true",      1);
  keywords.set(L"until",     1);
  keywords.set(L"while",     1);
  addKwGraph(L"function", L"end");
  addKwGraph(L"if", L"then");
  addKwGraph(L"then", L"end");
  addKwGraph(L"then", L"else");
  addKwGraph(L"then", L"elseif");
  addKwGraph(L"elseif", L"then");
  addKwGraph(L"else", L"end");
  addKwGraph(L"for", L"in");
  addKwGraph(L"for", L"do");
  addKwGraph(L"in", L"do");
  addKwGraph(L"while", L"do");
  addKwGraph(L"do", L"end");
  addKwGraph(L"repeat", L"until");
  kwBegin.set(L"function", 1);
  kwBegin.set(L"if", 1);
  kwBegin.set(L"for", 1);
  kwBegin.set(L"while", 1);
  kwBegin.set(L"do", 1);
  kwBegin.set(L"repeat", 1);
  kwEnd.set(L"end", 1);
  kwEnd.set(L"until", 1);
}


EditorSettings EditorSettings::defaultSettings = {
  { // lineNumbers
    4, // fields
    0xFFFFFF, // bgcolor
    16, // margin
  },
  { // font
    12, // size
    0, // flags
  },
  0xFFFFFF, // bgcolor
  { // colors
    0xAF912B, // line
    0x000000, // text
    0x000000, // number
    0xFF0000, // keyword
    0x1515A3, // string
    0x008000, // comment
    0xDDDDDD, // focusbg
  },
  { // selection
    0xFFD6AD, // activebg
    0xF1EBE5, // inactivebg
  },
  16, // bpOffset
  2, // tabSize
  0, // mode
};
EditorSettings EditorSettings::logSettings = {
  { // lineNumbers
    8, // fields
    0xFFFFFF, // bgcolor
    12, // margin
  },
  { // font
    12, // size
    0, // flags
  },
  0xFFFFFF, // bgcolor
  { // colors
    0x808080, // line
    0x000000, // text
    0x000000, // number
    0xFF0000, // keyword
    0x000000, // string
    0x000080, // error
    0xFFFFFF, // focusbg
  },
  { // selection
    0xFFD6AD, // activebg
    0xF1EBE5, // inactivebg
  },
  0, // bpOffset
  8, // tabSize
  1, // mode
};

POINT Editor::toPoint(int offset)
{
  POINT pt;
  pt.x = offset;
  pt.y = 0;
  while (pt.y < lines.length() - 1 && pt.x > lines[pt.y].text.length())
  {
    pt.x -= lines[pt.y].text.length() + 1;
    pt.y++;
  }
  if (pt.x > lines[pt.y].text.length())
    pt.x = lines[pt.y].text.length();
  return pt;
}
int Editor::fromPoint(POINT pt)
{
  if (pt.x < 0) pt.x = 0;
  if (pt.y >= lines.length()) pt.y = lines.length() - 1;
  int offset = 0;
  for (int i = 0; i < pt.y && i < lines.length(); i++)
    offset += lines[i].text.length() + 1;
  if (pt.y < 0 || pt.y >= lines.length())
    pt.x = 0;
  else if (pt.x > lines[pt.y].text.length())
    pt.x = lines[pt.y].text.length();
  return offset + pt.x;
}
void Editor::fixPoint(POINT& pt)
{
  if (pt.y < 0) pt.y = 0;
  if (pt.y >= lines.length()) pt.y = lines.length() - 1;
  if (pt.x < 0) pt.x = 0;
  if (pt.x > lines[pt.y].text.length()) pt.x = lines[pt.y].text.length();
}
POINT Editor::paramToPoint(uint32 param)
{
  POINT pt;
  pt.x = (GET_X_LPARAM(param) + chSize.cx / 2 - LeftMargin()) / chSize.cx + scrollPos.x;
  pt.y = GET_Y_LPARAM(param) / chSize.cy + scrollPos.y;
  return pt;
}
void Editor::updateExtent()
{
  extent.x = 0;
  extent.y = lines.length();
  for (int i = 0; i < lines.length(); i++)
    if (lines[i].text.length() > extent.x)
      extent.x = lines[i].text.length();

  RECT rc;
  GetClientRect(hWnd, &rc);
  SCROLLINFO si;
  memset(&si, 0, sizeof si);
  si.cbSize = sizeof si;
  si.fMask = SIF_PAGE | SIF_RANGE;
  si.nMin = 0;
  si.nPage = (rc.right - LeftMargin()) / chSize.cx;
  si.nMax = extent.x + 10;
  SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);
  si.nPage = rc.bottom / chSize.cy;
  si.nMax = extent.y - 1;
  SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
}
int Editor::getTextLength(bool crlf) const
{
  int count = (lines.length() - 1) * (crlf ? 2 : 1);
  for (int i = 0; i < lines.length(); i++)
    count += lines[i].text.length();
  return count;
}

void Editor::load(wchar_t const* name)
{
  File* file = File::wopen(name);
  if (file)
  {
    file->seek(0, SEEK_END);
    int length = file->tell();
    char* text = new char[length];
    file->seek(0, SEEK_SET);
    file->read(text, length);

    running = NULL;
    lines.clear();
    selStart = caret = 0;
    history.clear();
    historyPos = 0;
    origHistory = 0;
    scrollPos.x = 0;
    scrollPos.y = 0;

    char* str = text;
    int curpos = 0;
    while (curpos <= length)
    {
      if (curpos >= length || str[curpos] == '\r' || str[curpos] == '\n')
      {
        lines.push().text = WideString(str, curpos++);
        if (curpos < length && (str[curpos] == '\r' || str[curpos] == '\n') &&
            str[curpos] != str[curpos - 1])
          curpos++;
        str += curpos;
        length -= curpos;
        curpos = 0;
      }
      else
        curpos++;
    }
    delete[] text;

    if (lines.length() == 0)
      lines.push();
    updateCtx(0, lines.length() - 1);
    delete file;
    updateExtent();

    getParent()->notify(EN_MODIFIED, (uint32) this, 0);
  }
}
int Editor::updateCtx(int from, int to)
{
  if (settings->mode)
    return 0;
  int ctx = lines[from].ctx;
  int ctxType = lines[from].ctxType;
  for (int y = from; y <= to || (y < lines.length() &&
      (lines[y].ctx != ctx || lines[y].ctxType != ctxType)); y++)
  {
    lines[y].ctx = ctx;
    lines[y].ctxType = ctxType;
    lines[y].endComment = -1;
    wchar_t const* text = lines[y].text.c_str();
    int pos = 0;
    while (text[pos])
    {
      if (ctx)
      {
        if (text[pos] == ']')
        {
          pos++;
          int count = 0;
          while (text[pos] == '=' && count < ctxType)
            pos++, count++;
          if (text[pos] == ']' && count == ctxType)
          {
            pos++;
            ctx = 0;
            ctxType = 0;
          }
        }
        else
          pos++;
      }
      else
      {
        if (text[pos] == '[')
        {
          pos++;
          while (text[pos] == '=')
            pos++, ctxType++;
          if (text[pos] == '[')
          {
            pos++;
            ctx = ctxString;
          }
          else
            ctxType = 0;
        }
        else if (text[pos] == '-' && text[pos + 1] == '-')
        {
          int start = pos;
          pos += 2;
          if (text[pos] == '[')
          {
            pos++;
            while (text[pos] == '=')
              pos++, ctxType++;
            if (text[pos] == '[')
            {
              pos++;
              ctx = ctxComment;
            }
            else
            {
              ctxType = 0;
              lines[y].endComment = start;
              break;
            }
          }
          else
          {
            lines[y].endComment = start;
            break;
          }
        }
        else
          pos++;
      }
    }
  }
  return ctx;
}
static int getlength(wchar_t const* str)
{
  int len = 0;
  for (int i = 0; str[i]; i++)
  {
    if ((str[i] == '\r' || str[i] == '\n') &&
        (str[i + 1] == '\r' || str[i + 1] == '\n') && str[i + 1] != str[i])
      i++;
    len++;
  }
  return len;
}
int Editor::replace(int ibegin, int iend, WideString cstr, HistoryItem* mod, bool glue)
{
  if (running || settings->mode)
    return iend;
  wchar_t const* str = cstr.c_str();
  if (ibegin > iend)
  {
    int tmp = ibegin;
    ibegin = iend;
    iend = tmp;
  }
  POINT begin = toPoint(ibegin);
  POINT end = toPoint(iend);
  POINT oldEnd = end;

  if (mod)
  {
    mod->begin = ibegin;
    mod->end = ibegin + getlength(str);
    mod->text = substring(ibegin, iend);
  }
  else
  {
    HistoryItem h;
    h.begin = ibegin;
    h.end = ibegin + getlength(str);
    h.text = substring(ibegin, iend);
    h.glue = glue;

    bool push = true;
    if (historyPos < history.length())
      history.resize(historyPos);
    else if (historyPos > 0 && h.text.empty() && h.end == h.begin + 1 && !h.glue)
    {
      HistoryItem& p = history[historyPos - 1];
      if (p.text.empty() && h.begin == p.end)
      {
        push = false;
        p.end = h.end;
      }
    }
    if (push)
    {
      if (history.length() >= 256)
      {
        for (int i = 1; i < history.length(); i++)
          history[i - 1] = history[i];
        history[history.length() - 1] = h;
        if (origHistory >= 0)
          origHistory--;
      }
      else
        history.push(h);
    }
    historyPos = history.length();
    if (historyPos < origHistory)
      origHistory = -1;
  }

  if (begin.y == end.y)
  {
    Line ln = lines[begin.y];
    lines.insert(begin.y + 1, ln);
    end.y = begin.y + 1;
  }
  lines[begin.y].text.resize(begin.x);
  int curLine = begin.y;
  int curPos = 0;
  while (str[curPos])
  {
    if (str[curPos] == '\r' || str[curPos] == '\n')
    {
      if (curLine == begin.y)
        lines[curLine].text.append(str, curPos);
      else if (curLine < end.y)
        lines[curLine].text = WideString(str, curPos);
      else
      {
        Line& ln = lines.insert(curLine);
        ln.text = WideString(str, curPos);
        end.y++;
      }
      untabify(lines[curLine].text);
      if ((str[curPos + 1] == '\r' || str[curPos + 1] == '\n') && str[curPos] != str[curPos + 1])
        curPos++;
      str = str + curPos + 1;
      curPos = 0;
      curLine++;
    }
    else
      curPos++;
  }
  if (curLine < end.y)
  {
    if (curLine == begin.y)
      lines[curLine].text += str;
    else
      lines[curLine].text = str;
    untabify(lines[curLine].text);
    int ex = lines[curLine].text.length();
    lines[curLine].text += lines[end.y].text.substring(end.x);
    if (lines[end.y].breakpoint > 0)
      lines[curLine].breakpoint = 1;
    lines.remove(curLine + 1, end.y - curLine);
    end.y = curLine;
    end.x = ex;
  }
  else
  {
    lines[end.y].text.replace(0, end.x, str);
    end.x = wcslen(str);
    untabify(lines[end.y].text);
  }

  caret = selStart = fromPoint(end);
  updateCtx(begin.y, end.y);
  updateExtent();
  updateCaret();

  getParent()->notify(EN_MODIFIED, (uint32) this, 0);

  return caret;
}
WideString Editor::getSelection()
{
  int sela = (selStart < caret ? selStart : caret);
  int selb = (selStart > caret ? selStart : caret);
  return substring(sela, selb);
}
WideString Editor::substring(int a, int b)
{
  POINT pta = toPoint(a);
  POINT ptb = toPoint(b);
  WideString result;
  if (pta.y == ptb.y)
    result.append(lines[pta.y].text.c_str() + pta.x, ptb.x - pta.x);
  else
  {
    result.append(lines[pta.y].text.c_str() + pta.x, lines[pta.y].text.length() - pta.x);
    result.append(L"\r\n", 2);
    for (int y = pta.y + 1; y < ptb.y; y++)
    {
      result.append(lines[y].text, lines[y].text.length());
      result.append(L"\r\n", 2);
    }
    result.append(lines[ptb.y].text, ptb.x);
  }
  return result;
}

void Editor::placeCaret()
{
  if (GetFocus() == hWnd)
  {
    POINT pt = toPoint(caret);
    if (insertMode || caret != selStart)
      CreateCaret(hWnd, NULL, 1, chSize.cy);
    else
      CreateCaret(hWnd, NULL, chSize.cx, chSize.cy);
    SetCaretPos((pt.x - scrollPos.x) * chSize.cx + LeftMargin(), (pt.y - scrollPos.y) * chSize.cy);
    ShowCaret(hWnd);
  }
}
void Editor::doScroll(int x, int y)
{
  SCROLLINFO si;
  memset(&si, 0, sizeof si);
  si.cbSize = sizeof si;
  si.fMask = SIF_RANGE | SIF_PAGE;
  GetScrollInfo(hWnd, SB_HORZ, &si);
  if (x < si.nMin) x = si.nMin;
  if (x > si.nMax - si.nPage + 1) x = si.nMax - si.nPage + 1;
  GetScrollInfo(hWnd, SB_VERT, &si);
  if (y < si.nMin) y = si.nMin;
  if (y > si.nMax - si.nPage + 1) y = si.nMax - si.nPage + 1;
  si.fMask = SIF_POS;
  if (x != scrollPos.x)
  {
    si.nPos = x;
    SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);
  }
  if (y != scrollPos.y)
  {
    si.nPos = y;
    SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
  }
  if (x != scrollPos.x || y != scrollPos.y)
  {
    int deltaX = scrollPos.x - x;
    int deltaY = scrollPos.y - y;
    scrollPos.x = x;
    scrollPos.y = y;
    ScrollWindowEx(hWnd, chSize.cx * deltaX, chSize.cy * deltaY, NULL, NULL, NULL, NULL, SW_INVALIDATE);
    if (GetFocus() == hWnd)
    {
      POINT pt = toPoint(caret);
      SetCaretPos((pt.x - scrollPos.x) * chSize.cx + LeftMargin(), (pt.y - scrollPos.y) * chSize.cy);
    }
  }
}
void Editor::updateCaret()
{
  placeCaret();

  POINT pt = toPoint(caret);
  SCROLLINFO si;
  memset(&si, 0, sizeof si);
  si.cbSize = sizeof si;
  si.fMask = SIF_RANGE | SIF_PAGE;

  int xto = scrollPos.x;
  GetScrollInfo(hWnd, SB_HORZ, &si);
  if (pt.x < scrollPos.x)
    xto = (pt.x > 2 ? pt.x - 2 : 0);
  else if (pt.x >= scrollPos.x + si.nPage)
    xto = (pt.x - si.nPage + 10 < si.nMax ? pt.x - si.nPage + 10 : si.nMax);

  int yto = scrollPos.y;
  GetScrollInfo(hWnd, SB_VERT, &si);
  if (pt.y < scrollPos.y)
    yto = (pt.y > 2 ? pt.y - 2 : 0);
  else if (pt.y >= scrollPos.y + si.nPage)
    yto = (pt.y - si.nPage + 10 < si.nMax ? pt.y - si.nPage + 10 : si.nMax);

  doScroll(xto, yto);
  updateFocus();
  invalidate();
}
uint32 Editor::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  switch (message)
  {
  case WM_DESTROY:
    delete target;
    target = NULL;
    break;
  case WM_SETCURSOR:
    if (LOWORD(lParam) == HTCLIENT)
    {
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hWnd, &pt);
      if (pt.x < LeftMargin())
        SetCursor(cursors[cArrow]);
      else if (selStart != caret)
      {
        pt.x = (pt.x - LeftMargin()) / chSize.cx + scrollPos.x;
        pt.y = pt.y / chSize.cy + scrollPos.y;
        if (pt.y < 0 || pt.y >= lines.length() || pt.x < 0 || pt.x >= lines[pt.y].text.length())
          SetCursor(cursors[cBeam]);
        else
        {
          int offset = fromPoint(pt);
          int sela = (selStart < caret ? selStart : caret);
          int selb = (selStart < caret ? caret : selStart);
          if (offset >= sela && offset < selb)
            SetCursor(cursors[cArrow]);
          else
            SetCursor(cursors[cBeam]);
        }
      }
      else
        SetCursor(cursors[cBeam]);
    }
    else
      SetCursor(cursors[cArrow]);
    return TRUE;
  case WM_ERASEBKGND:
    return TRUE;
  case WM_PAINT:
    paint();
    return 0;
  case WM_SIZE:
    if (hBitmap)
    {
      int wd = LOWORD(lParam);
      int ht = HIWORD(lParam);
      if (wd < 10) wd = 10;
      if (ht < 10) ht = 10;
      hBitmap = CreateCompatibleBitmap(hDC, wd, ht);
      SelectObject(hDC, hBitmap);
    }
    updateExtent();
    return 0;
  case WM_SETFOCUS:
    placeCaret();
    invalidate();
    getParent()->notify(EN_FOCUSED, (uint32) this, 0);
    return 0;
  case WM_KILLFOCUS:
    DestroyCaret();
    updateCaret();
    invalidate();
    return 0;
  case WM_LBUTTONDBLCLK:
    {
      POINT pt = paramToPoint(lParam);
      fixPoint(pt);
      int ptStart = wordEnd(lines[pt.y].text.c_str(), pt.x, -1);
      int ptEnd = wordEnd(lines[pt.y].text.c_str(), pt.x, 1);
      int offset = fromPoint(pt);
      selStart = offset - (pt.x - ptStart);
      caret = offset + (ptEnd - pt.x);
      updateCaret();
    }
    return 0;
  case WM_LBUTTONDOWN:
    if (int(GET_X_LPARAM(lParam)) < int(settings->bpOffset - scrollPos.x * chSize.cx))
    {
      POINT pt = paramToPoint(lParam);
      toggleBreakpoint(pt.y);
    }
    else
    {
      POINT pt = paramToPoint(lParam);
      int offset = fromPoint(pt);
      int sela = (selStart < caret ? selStart : caret);
      int selb = (selStart > caret ? selStart : caret);
      if (offset >= sela && offset < selb)
      {
        dragop = 1;
        uint32 fx = DoDragDropEx(CF_UNICODETEXT, CreateGlobalText(getSelection()),
            DROPEFFECT_MOVE | DROPEFFECT_COPY, hWnd);
        if (fx == DROPEFFECT_NONE)
          dragop = 0;
        //else if (fx != DROPEFFECT_COPY && dragop != 2)
        //  replace(sela, selb, "");
      }
      else
        SetCapture(hWnd);
      if (dragop == 0)
      {
        caret = offset;
        if (!(wParam & MK_SHIFT))
          selStart = caret;
      }
      dragop = 0;
      if (GetFocus() != hWnd)
        SetFocus(hWnd);
      updateCaret();
    }
    return 0;
  case WM_RBUTTONDOWN:
    if (int(GET_X_LPARAM(lParam)) >= int(settings->bpOffset - scrollPos.x * chSize.cx))
    {
      POINT pt = paramToPoint(lParam);
      int offset = fromPoint(pt);
      int sela = (selStart < caret ? selStart : caret);
      int selb = (selStart > caret ? selStart : caret);
      if (offset < sela || offset >= selb)
        caret = selStart = offset;
      if (GetFocus() != hWnd)
        SetFocus(hWnd);
      updateCaret();
    }
    return 0;
  case WM_MOUSEMOVE:
    if (GetCapture() == hWnd && (wParam & MK_LBUTTON))
    {
      POINT pt = paramToPoint(lParam);
      caret = fromPoint(pt);
      updateCaret();
    }
    return 0;
  case WM_LBUTTONUP:
    ReleaseCapture();
    return 0;
  case WM_CHAR:
    if (iswprint(wParam) && (GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0)
    {
      if (caret == selStart && !insertMode && caret < getTextLength())
        replace(caret, caret + 1, WideString((wchar_t) wParam));
      else
        replace(selStart, caret, WideString((wchar_t) wParam));
    }
    return 0;
  case WM_VSCROLL:
    {
      SCROLLINFO si;
      memset(&si, 0, sizeof si);
      si.cbSize = sizeof si;
      si.fMask = SIF_ALL;
      GetScrollInfo(hWnd, SB_VERT, &si);
      switch (LOWORD(wParam))
      {
      case SB_TOP:
        si.nPos = si.nMin;
        break;
      case SB_BOTTOM:
        si.nPos = si.nMax;
        break;
      case SB_LINEUP:
        si.nPos--;
        break;
      case SB_LINEDOWN:
        si.nPos++;
        break;
      case SB_PAGEUP:
        si.nPos -= si.nPage;
        break;
      case SB_PAGEDOWN:
        si.nPos += si.nPage;
        break;
      case SB_THUMBTRACK:
        si.nPos = si.nTrackPos;
        break;
      }
      doScroll(scrollPos.x, si.nPos);
    }
    return 0;
  case WM_HSCROLL:
    {
      SCROLLINFO si;
      memset(&si, 0, sizeof si);
      si.cbSize = sizeof si;
      si.fMask = SIF_ALL;
      GetScrollInfo(hWnd, SB_HORZ, &si);
      switch (LOWORD(wParam))
      {
      case SB_LEFT:
        si.nPos = si.nMin;
        break;
      case SB_RIGHT:
        si.nPos = si.nMax;
        break;
      case SB_LINELEFT:
        si.nPos--;
        break;
      case SB_LINERIGHT:
        si.nPos++;
        break;
      case SB_PAGELEFT:
        si.nPos -= si.nPage;
        break;
      case SB_PAGERIGHT:
        si.nPos += si.nPage;
        break;
      case SB_THUMBTRACK:
        si.nPos = si.nTrackPos;
        break;
      }
      doScroll(si.nPos, scrollPos.y);
    }
    return 0;
  case WM_MOUSEWHEEL:
    {
      int step;
      SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &step, 0);
      if (step < 0)
        step = 3;
      scrollAccum.y += GET_WHEEL_DELTA_WPARAM(wParam) * step;
      doScroll(scrollPos.x, scrollPos.y - scrollAccum.y / WHEEL_DELTA);
      scrollAccum.y %= WHEEL_DELTA;
    }
    return 0;
  case WM_MOUSEHWHEEL:
    {
      scrollAccum.x += GET_WHEEL_DELTA_WPARAM(wParam) * 4;
      doScroll(scrollPos.x + scrollAccum.x / WHEEL_DELTA, scrollPos.y);
      scrollAccum.x %= WHEEL_DELTA;
    }
    return 0;
  case WM_DRAGOVER:
    {
      if (running || settings->mode)
        return TRUE;
      POINT pt = paramToPoint(lParam);
      dropPos = fromPoint(pt);

      RECT rc;
      GetClientRect(hWnd, &rc);
      int xto = scrollPos.x;
      if (pt.x < 10)
        xto--;
      else if (pt.x > rc.right - 10)
        xto++;
      int yto = scrollPos.y;
      if (pt.y < 10)
        yto--;
      else if (pt.y > rc.bottom - 10)
        yto++;
      doScroll(xto, yto);

      int sela = (selStart < caret ? selStart : caret);
      int selb = (selStart > caret ? selStart : caret);
      if (dropPos > sela && dropPos < selb)
        return TRUE;
      else
      {
        fixPoint(pt);
        CreateCaret(hWnd, NULL, 2, chSize.cy);
        SetCaretPos((pt.x - scrollPos.x) * chSize.cx + LeftMargin(), (pt.y - scrollPos.y) * chSize.cy);
        ShowCaret(hWnd);
      }
    }
    return 0;
  case WM_DRAGLEAVE:
    dropPos = 0;
    updateCaret();
    return 0;
  case WM_DRAGDROP:
    if (running || settings->mode)
      return DROPEFFECT_NONE;
    if (dragop)
    {
      dragop = 2;
      int sela = (selStart < caret ? selStart : caret);
      int selb = (selStart > caret ? selStart : caret);
      if (dropPos < sela || dropPos > selb)
      {
        WideString text = getSelection();
        if (lParam != DROPEFFECT_COPY)
        {
          replace(sela, selb, L"");
          if (dropPos > selb)
            dropPos -= (selb - sela);
          caret = replace(dropPos, dropPos, text, NULL, true);
        }
        else
          caret = replace(dropPos, dropPos, text);
        selStart = dropPos;
      }
    }
    else
    {
      caret = replace(dropPos, dropPos, GetGlobalTextWide((HGLOBAL) wParam));
      selStart = dropPos;
      return DROPEFFECT_COPY;
    }
    return lParam;
  case WM_SYSKEYDOWN:
  case WM_KEYDOWN:
    onKey(wParam);
    return 0;
  case WM_COMMAND:
    switch (LOWORD(wParam))
    {
    case ID_EDIT_UNDO:
      {
        bool glue = true;
        bool first = true;
        while (glue && historyPos > 0)
        {
          HistoryItem& h = history[--historyPos];
          replace(h.begin, h.end, h.text, &h);
          glue = h.glue;
          h.glue = !first;
          first = false;
        }
      }
      break;
    case ID_EDIT_REDO:
      {
        bool glue = true;
        bool first = true;
        while (glue && historyPos < history.length())
        {
          HistoryItem& h = history[historyPos++];
          replace(h.begin, h.end, h.text, &h);
          glue = h.glue;
          h.glue = !first;
          first = false;
        }
      }
      break;
    case ID_EDIT_SELECTALL:
      selStart = 0;
      caret = getTextLength();
      updateCaret();
      break;
    case ID_EDIT_COPY:
      if (caret != selStart)
        SetClipboard(CF_UNICODETEXT, CreateGlobalText(getSelection()));
      else
      {
        POINT pt = toPoint(caret);
        pt.x = 0;
        int start = fromPoint(pt);
        if (pt.y < lines.length() - 1)
          pt.y++;
        else
          pt.x = lines[pt.y].text.length();
        int end = fromPoint(pt);
        if (pCopyLine)
          pCopyLine->Release();
        pCopyLine = SetClipboard(CF_UNICODETEXT, CreateGlobalText(substring(start, end)));
        if (pCopyLine)
          pCopyLine->AddRef();
      }
      break;
    case ID_EDIT_CUT:
      if (caret != selStart)
      {
        SetClipboard(CF_UNICODETEXT, CreateGlobalText(getSelection()));
        replace(selStart, caret, L"");
      }
      else
      {
        POINT pt = toPoint(caret);
        POINT save = pt;
        pt.x = 0;
        int start = fromPoint(pt);
        if (pt.y < lines.length() - 1)
          pt.y++;
        else
          pt.x = lines[pt.y].text.length();
        int end = fromPoint(pt);
        if (pCopyLine)
          pCopyLine->Release();
        pCopyLine = SetClipboard(CF_UNICODETEXT, CreateGlobalText(substring(start, end)));
        if (pCopyLine)
          pCopyLine->AddRef();
        replace(start, end, L"");
        caret = selStart = fromPoint(save);
        updateCaret();
      }
      break;
    case ID_EDIT_PASTE:
      {
        ClipboardReader reader(CF_UNICODETEXT);
        if (reader.getData())
        {
          if (OleIsCurrentClipboard(pCopyLine) == S_OK)
          {
            POINT pt = toPoint(caret);
            pt.x = 0;
            caret = selStart = fromPoint(pt);
          }
          selStart = caret = replace(selStart, caret, GetGlobalTextWide(reader.getData()));
          updateCaret();
        }
      }
      break;
    case ID_DEBUG_BREAKPOINT:
      {
        POINT pt = toPoint(caret);
        toggleBreakpoint(pt.y);
      }
      break;
    default:
      return M_UNHANDLED;
    }
    return 0;
  }
  return M_UNHANDLED;
}
static inline char chType(wchar_t c)
{
  if (iswspace(c))
    return 1;
  else if (iswalnum(c) || c == '_')
    return 2;
  else
    return 0;
}
int Editor::wordEnd(wchar_t const* str, int pos, int dir)
{
  if (dir > 0)
  {
    if (str[pos] == 0)
      return pos;
    char t = chType(str[pos]);
    do {pos++;} while(str[pos] && chType(str[pos]) == t);
    return pos;
  }
  else
  {
    if (str[pos] == 0)
      pos--;
    char t = chType(str[pos]);
    while (pos > 0 && chType(str[pos - 1]) == t)
      pos--;
    return pos;
  }
}
int Editor::getWordSize(int pos, int dir)
{
  if (pos == 0 && dir < 0)
    return 0;
  if (dir < 0)
    pos--;
  POINT pt = toPoint(pos);
  int size = (wordEnd(lines[pt.y].text.c_str(), pt.x, dir) - pt.x) * dir + (dir < 0 ? 1 : 0);
  if (size < 1) size = 1;
  return size;
}
void Editor::untabify(WideString& text)
{
  int width = settings->tabSize;
  for (int i = 0; text[i]; i++)
  {
    if (text[i] == '\t')
    {
      text.replace(i, ' ');
      //int extra = (width - (i % width)) - 1;
      //for (int i = 0; i < extra; i++)
      //  text.insert(i, ' ');
    }
  }
}

Editor::Editor(Frame* parent, EditorSettings* set, int id)
  : WindowFrame(parent)
  , hFont(FontSys::getFont(set->font.size, L"Courier New", set->font.flags))
  , hFontIt(FontSys::getFont(set->font.size, L"Courier New", set->font.flags | FONT_ITALIC))
  , hFontBf(FontSys::getFont(set->font.size, L"Courier New", set->font.flags | FONT_BOLD))
  , caret(0)
  , selStart(0)
  , insertMode(true)
  , target(NULL)
  , historyPos(0)
  , origHistory(0)
  , running(NULL)
  , dragop(0)
  , hDC(NULL)
  , hBitmap(NULL)
  , dropPos(0)
  , currentLine(-1)
  , settings(set)
  , pCopyLine(NULL)
{
  cursors[cArrow] = LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW));
  cursors[cBeam] = LoadCursor(NULL, MAKEINTRESOURCE(IDC_IBEAM));
  icons[iBreakpoint] = (HICON) LoadImage(getInstance(),
    MAKEINTRESOURCE(IDI_BREAKPOINT), IMAGE_ICON, 16, 16, 0);
  icons[iCurline] = (HICON) LoadImage(getInstance(),
    MAKEINTRESOURCE(IDI_CURLINE), IMAGE_ICON, 16, 16, 0);
  chSize = FontSys::getTextSize(hFont, L" ");
  scrollPos.x = scrollPos.y = 0;
  scrollAccum.x = scrollAccum.y;
  extent.x = extent.y = 0;
  lines.push();
  initkw();

  if (WNDCLASSEX* wcx = createclass(L"iLuaEditorWindow"))
  {
    wcx->style |= CS_DBLCLKS;
    RegisterClassEx(wcx);
  }
  create(L"", WS_CHILD | WS_TABSTOP | WS_HSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE);
  setId(id);

  enableTooltips();

  target = new DropTarget(this, CF_UNICODETEXT, DROPEFFECT_COPY | DROPEFFECT_MOVE);
}
Editor::~Editor()
{
  if (pCopyLine)
    pCopyLine->Release();
  DeleteObject(hBitmap);
  DeleteDC(hDC);
}
void Editor::toggleBreakpoint(int y)
{
  if (lines[y].breakpoint >= 0)
    lines[y].breakpoint = 1 - lines[y].breakpoint;
  else if (!running)
    lines[y].breakpoint = 1;
  else
  {
    while (y < lines.length() && lines[y].breakpoint < 0)
      y++;
    if (y >= lines.length())
      return;
    lines[y].breakpoint = 1;
    POINT ncp;
    ncp.x = 0;
    ncp.y = y;
    selStart = caret = fromPoint(ncp);
    placeCaret();
  }
  getParent()->notify(EN_SETBREAKPOINT, y, (uint32) this);
  invalidate();
}
void Editor::setCursor(int line, int col)
{
  POINT pt;
  pt.x = col;
  pt.y = line;
  selStart = caret = fromPoint(pt);
  updateCaret();
}
void Editor::selectLine(int line)
{
  if (line < 0 || line >= lines.length())
    return;
  POINT pt;
  pt.x = 0;
  pt.y = line;
  selStart = fromPoint(pt);
  caret = selStart + lines[line].text.length();
  updateCaret();
}

void Editor::save(wchar_t const* name)
{
  File* file = File::wopen(name, File::REWRITE);
  if (file)
  {
    for (int y = 0; y < lines.length(); y++)
    {
      String line(lines[y].text);
      file->write(line.c_str(), line.length());
      if (y < lines.length() - 1)
        file->write("\r\n", 2);
    }
    delete file;
    setModified(false);
  }
}

void Editor::addLogMessage(char const* text, uint64 timestamp, uint32 type)
{
  int len = getTextLength();
  POINT ptCaret = toPoint(caret);
  POINT ptSelStart = toPoint(selStart);
  if (lines.length() == 1 && lines[0].timestamp == 0)
    lines.pop();
  else if (lines.length() > 1024)
  {
    lines.remove(0, 1);
    ptCaret.y--;
    ptSelStart.y--;
  }
  int pos = 0;
  while (true)
  {
    if (text[pos] == '\r' || text[pos] == '\n' || text[pos] == 0)
    {
      Line& ln = lines.push();
      ln.text = WideString(text, pos);
      for (int i = 0; i < ln.text.length(); i++)
        if (ln.text[i] == '\t')
          ln.text.replace(i, 1, WideString(" ") * (settings->tabSize - (i % settings->tabSize)));
      ln.ctx = type;
      ln.timestamp = timestamp;
      if (text[pos] == 0)
        break;
      pos++;
      if ((text[pos] == '\r' || text[pos] == '\n') && text[pos - 1] != text[pos])
        pos++;
      text += pos;
      pos = 0;
    }
    else
      pos++;
  }
  if (selStart == caret && caret == len)
    selStart = caret = getTextLength();
  else
  {
    caret = fromPoint(ptCaret);
    selStart = fromPoint(ptSelStart);
  }
  updateExtent();
  updateCaret();
}
void Editor::setBreakLines(Array<int>& bp)
{
  if (bp.length() == 0)
    return;
  for (int i = 0; i < lines.length(); i++)
    lines[i].breakpoint = (lines[i].breakpoint > 0 ? -2 : -1);
  for (int i = 0; i < bp.length(); i++)
  {
    int y = bp[i];
    if (y >= 0 && y < lines.length())
      lines[y].breakpoint = (lines[y].breakpoint == -2 ? 1 : 0);
  }
  int val = 0;
  for (int i = 0; i < lines.length(); i++)
  {
    if (lines[i].breakpoint == -2)
    {
      lines[i].breakpoint = -1;
      val = 1;
    }
    else if (val && lines[i].breakpoint >= 0)
    {
      lines[i].breakpoint = 1;
      val = 0;
    }
  }
}
bool Editor::tryClose(char const* path, wchar_t const* title)
{
  if (modified())
  {
    WideString wpath(path);
    switch (MessageBox(hWnd, WideString::format(L"Do you want to save changes to %s?", title),
        L"iLua Core", MB_YESNOCANCEL))
    {
    case IDYES:
      save(wpath);
    case IDNO:
      return true;
    case IDCANCEL:
      return false;
    }
  }
  return true;
}

int Editor::toolHitTest(POINT pt, ToolInfo* ti)
{
  if (!running || running->getDebugLevel() < 0)
    return -1;
  pt.x = (pt.x + chSize.cx / 2 - LeftMargin()) / chSize.cx + scrollPos.x;
  pt.y = pt.y / chSize.cy + scrollPos.y;
  fixPoint(pt);
  int offset = fromPoint(pt);
  int sela = (selStart < caret ? selStart : caret);
  int selb = (selStart > caret ? selStart : caret);
  if (offset < sela || offset >= selb)
  {
    int ptStart = wordEnd(lines[pt.y].text.c_str(), pt.x, -1);
    int ptEnd = wordEnd(lines[pt.y].text.c_str(), pt.x, 1);
    int offset = fromPoint(pt);
    sela = offset - (pt.x - ptStart);
    selb = offset + (ptEnd - pt.x);
  }
  int index = -1;
  if (offset >= sela && offset < selb)
  {
    String text(substring(sela, selb));
    lua_State* L = running->getEngine()->lock();
    int res = luae::getvalue(L, running->getDebugLevel(), text);
    if (res != luae::eFail && !lua_isnil(L, -1))
    {
      ti->text = luae::towstring(L, -1, true);
      lua_pop(L, 1);
      POINT pta = toPoint(sela);
      POINT ptb = toPoint(selb);
      int x0 = (pta.y == pt.y ? pta.x : 0);
      int x1 = (ptb.y == pt.y ? ptb.x : lines[pt.y].text.length());
      ti->rc.top = (pt.y - scrollPos.y) * chSize.cy;
      ti->rc.bottom = (pt.y + 1 - scrollPos.y) * chSize.cy;
      ti->rc.left = (x0 - scrollPos.x) * chSize.cx + LeftMargin();
      ti->rc.right = (x1 - scrollPos.x) * chSize.cx + LeftMargin();
      index = sela;
    }
    else if (res != luae::eFail)
      lua_pop(L, 1);
    running->getEngine()->unlock();
  }

  return index;
}
void Editor::clear()
{
  running = NULL;
  caret = 0;
  selStart = 0;
  insertMode = true;
  scrollPos.x = scrollPos.y = 0;
  scrollAccum.x = scrollAccum.y = 0;
  extent.x = extent.y = 0;
  dragop = 0;
  dropPos = 0;
  currentLine = -1;
  history.clear();
  historyPos = 0;
  origHistory = 0;
  lines.clear();
  lines.push();
}

static inline wchar_t isBracket(wchar_t c)
{
  switch (c)
  {
  case '(': return ')';
  case ')': return '(';
  case '[': return ']';
  case ']': return '[';
  case '{': return '}';
  case '}': return '{';
  default: return 0;
  }
}
WideString Editor::prevKey(POINT& pt, bool bracket)
{
  pt.x--;
  int ctx = 0;
  int ctxType = 0;
  while (pt.y >= 0)
  {
    if (pt.x < 0)
    {
      pt.y--;
      if (pt.y < 0) break;
      pt.x = lines[pt.y].text.length() - 1;
      if (lines[pt.y].endComment >= 0)
        pt.x = lines[pt.y].endComment;
    }
    else
    {
      wchar_t const* text = lines[pt.y].text.c_str();
      if (ctx)
      {
        if (text[pt.x] == '[')
        {
          pt.x--;
          int count = 0;
          while (pt.x >= 0 && text[pt.x] == '=' && count < ctxType)
            pt.x--, count++;
          if (pt.x >= 0 && text[pt.x] == '[' && count == ctxType)
          {
            pt.x--;
            if (pt.x >= 1 && text[pt.x] == '-' && text[pt.x - 1] == '-')
              pt.x -= 2;
            ctx = 0;
            ctxType = 0;
          }
        }
        else
          pt.x--;
      }
      else
      {
        if (text[pt.x] == ']')
        {
          int start = pt.x;
          pt.x--;
          while (pt.x >= 0 && text[pt.x] == '=')
            pt.x--, ctxType++;
          if (pt.x >= 0 && text[pt.x] == ']')
          {
            pt.x--;
            ctx = ctxString;
          }
          else
          {
            if (bracket)
            {
              pt.x = start;
              return L"]";
            }
            ctxType = 0;
          }
        }
        else if (text[pt.x] == '"' || text[pt.x] == '\'')
        {
          wchar_t end = text[pt.x--];
          while (pt.x >= 0)
          {
            if (text[pt.x] == end)
            {
              int cur = pt.x;
              while (cur > 0 && text[cur - 1] == '\\')
                cur--;
              if (((pt.x - cur) & 1) == 0)
                break;
            }
            pt.x--;
          }
          pt.x--;
        }
        else if (bracket && isBracket(text[pt.x]))
          return WideString(text[pt.x]);
        else if (!bracket && iswalpha(text[pt.x]))
        {
          int end = pt.x + 1;
          while (pt.x > 0 && iswalpha(text[pt.x - 1]))
            pt.x--;
          while (iswalpha(text[end]))
            end++;
          WideString kw = lines[pt.y].text.substring(pt.x, end);
          if (keywords.has(kw))
            return kw;
          pt.x--;
        }
        else
          pt.x--;
      }
    }
  }
  return L"";
}
WideString Editor::nextKey(POINT& pt, bool bracket)
{
  pt.x++;
  int ctx = 0;
  int ctxType = 0;
  while (pt.y < lines.length())
  {
    if (pt.x >= lines[pt.y].text.length())
    {
      pt.x = 0;
      pt.y++;
    }
    else
    {
      wchar_t const* text = lines[pt.y].text.c_str();
      if (ctx)
      {
        if (text[pt.x] == ']')
        {
          pt.x++;
          int count = 0;
          while (text[pt.x] == '=' && count < ctxType)
            pt.x++, count++;
          if (text[pt.x] == ']' && count == ctxType)
          {
            pt.x++;
            ctx = 0;
            ctxType = 0;
          }
        }
        else
          pt.x++;
      }
      else
      {
        if (text[pt.x] == '[')
        {
          int start = pt.x;
          pt.x++;
          while (text[pt.x] == '=')
            pt.x++, ctxType++;
          if (text[pt.x] == '[')
          {
            pt.x++;
            ctx = ctxString;
          }
          else
          {
            if (bracket)
            {
              pt.x = start;
              return L"[";
            }
            ctxType = 0;
          }
        }
        else if (text[pt.x] == '-' && text[pt.x + 1] == '-')
        {
          pt.x += 2;
          if (text[pt.x] == '[')
          {
            pt.x++;
            while (text[pt.x] == '=')
              pt.x++, ctxType++;
            if (text[pt.x] == '[')
            {
              pt.x++;
              ctx = ctxComment;
            }
            else
            {
              ctxType = 0;
              pt.x = lines[pt.y].text.length();
            }
          }
          else
          {
            ctxType = 0;
            pt.x = lines[pt.y].text.length();
          }
        }
        else if (text[pt.x] == '"' || text[pt.x] == '\'')
        {
          wchar_t end = text[pt.x++];
          while (text[pt.x] && text[pt.x] != end)
          {
            if (text[pt.x] == '\\') pt.x++;
            pt.x++;
          }
          if (text[pt.x])
            pt.x++;
        }
        else if (bracket && isBracket(text[pt.x]))
          return WideString(text[pt.x]);
        else if (!bracket && iswalpha(text[pt.x]) && (pt.x == 0 || !iswalpha(text[pt.x - 1])))
        {
          int end = pt.x + 1;
          while (iswalpha(text[end]))
            end++;
          WideString kw = lines[pt.y].text.substring(pt.x, end);
          if (keywords.has(kw))
            return kw;
          pt.x = end;
        }
        else
          pt.x++;
      }
    }
  }
  return L"";
}
void Editor::updateFocus()
{
  focus.clear();
  if (caret != selStart)
    return;
  POINT pt = toPoint(caret);

  int ctx = lines[pt.y].ctx;
  int ctxType = lines[pt.y].ctxType;
  wchar_t const* text = lines[pt.y].text.c_str();
  int pos = 0;
  while (pos < pt.x && text[pos])
  {
    if (ctx)
    {
      if (text[pos] == ']')
      {
        pos++;
        int count = 0;
        while (text[pos] == '=' && count < ctxType)
          pos++, count++;
        if (text[pos] == ']' && count == ctxType)
        {
          pos++;
          ctx = 0;
          ctxType = 0;
        }
      }
      else
        pos++;
    }
    else
    {
      if (text[pos] == '[')
      {
        pos++;
        while (text[pos] == '=')
          pos++, ctxType++;
        if (text[pos] == '[')
        {
          pos++;
          ctx = ctxString;
        }
        else
          ctxType = 0;
      }
      else if (text[pos] == '-' && text[pos + 1] == '-')
      {
        pos += 2;
        if (text[pos] == '[')
        {
          pos++;
          while (text[pos] == '=')
            pos++, ctxType++;
          if (text[pos] == '[')
          {
            pos++;
            ctx = ctxComment;
          }
          else
            return;
        }
        else
          return;
      }
      else if (text[pos] == '"' || text[pos] == '\'')
      {
        wchar_t end = text[pos++];
        while (text[pos] && text[pos] != end)
        {
          if (text[pos] == '\\') pos++;
          if (text[pos]) pos++;
        }
        if (text[pos])
          pos++;
      }
      else
        pos++;
    }
  }
  if (pos > pt.x || ctx) return;
  if (text[pt.x] == '[')
  {
    int pos = pt.x + 1;
    while (text[pos] == '=')
      pos++;
    if (text[pos] == '[')
      return;
  }

  int start = pt.x;
  int type = 0;
  WideString kw;
  if (isBracket(lines[pt.y].text[pt.x]))
    type = 1;
  else
  {
    int end = start;
    while (start > 0 && iswalpha(lines[pt.y].text[start - 1]))
      start--;
    while (iswalpha(lines[pt.y].text[end]))
      end++;
    kw = lines[pt.y].text.substring(start, end);
    if (keywords.has(kw))
      type = -1;
  }
  if (type == 0 && pt.x > 0 && isBracket(lines[pt.y].text[pt.x - 1]))
  {
    start = pt.x - 1;
    type = 1;
  }
  pt.x = start;
  if (type == 1)
  {
    int stack = 1;
    wchar_t fwd = lines[pt.y].text[start];
    wchar_t rev = isBracket(fwd);
    bool forward = (fwd == '(' || fwd == '[' || fwd == '{');
    while (stack > 0)
    {
      WideString brk = (forward ? nextKey(pt, true) : prevKey(pt, true));
      if (brk.length() == 0 || pt.y < 0 || pt.y >= lines.length())
        break;
      if (brk[0] == fwd)
        stack++;
      else if (brk[0] == rev)
        stack--;
    }
    if (stack == 0)
    {
      focus.push(fromPoint(pt));
      pt = toPoint(caret); pt.x = start;
      focus.push(fromPoint(pt));
    }
  }
  else if (type == -1 && kwInGraph.has(kw))
  {
    Array<WideString> stk;
    stk.push(kw);
    POINT save = pt;
    while (stk.length() && pt.y < lines.length())
    {
      WideString next = nextKey(pt, false);
      if (next.length() == 0) break;
      if (inKwGraph(stk.top(), next))
      {
        if (stk.length() == 1)
          focus.push(fromPoint(pt));
        stk.top() = next;
      }
      else if (kwBegin.has(next))
        stk.push(next);
      if (kwEnd.has(stk.top()))
        stk.pop();
    }
    pt = save;
    stk.clear();
    stk.push(kw);
    while (stk.length() && pt.y >= 0)
    {
      WideString prev = prevKey(pt, false);
      if (prev.length() == 0) break;
      if (inKwGraph(prev, stk.top()))
      {
        if (stk.length() == 1)
          focus.push(fromPoint(pt));
        stk.top() = prev;
      }
      else if (kwEnd.has(prev))
        stk.push(prev);
      if (kwBegin.has(stk.top()) && (stk.top() != L"do" || stk.length() > 1))
        stk.pop();
    }
    if (focus.length())
    {
      pt = toPoint(caret); pt.x = start;
      focus.push(fromPoint(pt));
    }
  }
}
