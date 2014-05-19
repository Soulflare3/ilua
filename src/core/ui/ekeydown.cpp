#include "editor.h"
#include "core/api/engine.h"

void Editor::onKey(uint32 wParam)
{
  switch (wParam)
  {
  case VK_HOME:
    {
      if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
        caret = 0;
      else
      {
        POINT pt = toPoint(caret);
        wchar_t const* text = lines[pt.y].text.c_str();
        int first = 0;
        while (text[first] && iswspace(text[first]))
          first++;
        if (pt.x == first)
          pt.x = 0;
        else
          pt.x = first;
        caret = fromPoint(pt);
      }
      if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000))
        selStart = caret;
      updateCaret();
    }
    break;
  case VK_END:
    {
      if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
        caret = getTextLength();
      else
      {
        POINT pt = toPoint(caret);
        pt.x = lines[pt.y].text.length();
        caret = fromPoint(pt);
      }
      if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000))
        selStart = caret;
      updateCaret();
    }
    break;
  case VK_TAB:
    {
      bool swap = (selStart < caret);
      bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
      POINT pt1 = toPoint(selStart < caret ? selStart : caret);
      POINT pt2 = toPoint(selStart < caret ? caret : selStart);
      if (pt1.y != pt2.y)
      {
        WideString tab = WideString(" ") * settings->tabSize;
        for (int line = pt1.y; line <= pt2.y; line++)
        {
          if (line < pt2.y || pt2.x != 0)
          {
            POINT pt;
            pt.x = 0;
            pt.y = line;
            int pos = fromPoint(pt);
            if (shift)
            {
              int count = 0;
              for (int i = 0; lines[line].text[i] == ' ' && count < settings->tabSize; i++)
                count++;
              if (count)
              {
                replace(pos, pos + count, L"", NULL, line != pt1.y);
                if (line == pt1.y)
                  pt1.x -= count;
                if (line == pt2.y)
                  pt2.x -= count;
              }
            }
            else
              replace(pos, pos, tab, NULL, line != pt1.y);
          }
        }
        if (!shift)
        {
          if (pt1.y != 0)
            pt1.x += settings->tabSize;
          if (pt2.y != 0)
            pt2.x += settings->tabSize;
        }
        else
        {
          if (pt1.y < 0)
            pt1.x = 0;
          if (pt2.y < 0)
            pt2.x = 0;
        }
        caret = fromPoint(pt1);
        selStart = fromPoint(pt2);
        if (swap)
        {
          int temp = caret;
          caret = selStart;
          selStart = temp;
        }
        updateCaret();
      }
      else if (!shift)
      {
        WideString space(" ");
        replace(selStart, caret, space * (settings->tabSize - (pt1.x % settings->tabSize)));
      }
    }
    break;
  case VK_BACK:
    if (selStart != caret)
      replace(selStart, caret, L"");
    else if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
      replace(caret - getWordSize(caret, -1), caret, L"");
    else if (caret > 0)
      replace(caret - 1, caret, L"");
    break;
  case VK_DELETE:
    if (selStart != caret)
      replace(selStart, caret, L"");
    else if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
      replace(caret, caret + getWordSize(caret, 1), L"");
    else if (caret < getTextLength())
      replace(caret, caret + 1, L"");
    break;
  case VK_LEFT:
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
      caret -= getWordSize(caret, -1);
    else if (caret > 0)
      caret--;
    if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000))
      selStart = caret;
    updateCaret();
    break;
  case VK_RIGHT:
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
      caret += getWordSize(caret, 1);
    else if (caret < getTextLength())
      caret++;
    if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000))
      selStart = caret;
    updateCaret();
    break;
  case VK_UP:
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
      doScroll(scrollPos.x, scrollPos.y - 1);
    else
    {
      POINT pt = toPoint(caret);
      pt.y--;
      pt.x = caretX;
      caret = fromPoint(pt);
      if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000))
        selStart = caret;
      updateCaret();
      caretX = pt.x;
    }
    break;
  case VK_DOWN:
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
      doScroll(scrollPos.x, scrollPos.y + 1);
    else
    {
      POINT pt = toPoint(caret);
      pt.y++;
      pt.x = caretX;
      caret = fromPoint(pt);
      if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000))
        selStart = caret;
      updateCaret();
      caretX = pt.x;
    }
    break;
  case VK_PRIOR:
    {
      SCROLLINFO si;
      memset(&si, 0, sizeof si);
      si.cbSize = sizeof si;
      si.fMask = SIF_PAGE | SIF_RANGE;
      GetScrollInfo(hWnd, SB_VERT, &si);
      doScroll(scrollPos.x, scrollPos.y - si.nPage > 0 ? scrollPos.y - si.nPage : 0);

      POINT pt = toPoint(caret);
      pt.y -= si.nPage;
      caret = fromPoint(pt);
      if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000))
        selStart = caret;
      updateCaret();
    }
    break;
  case VK_NEXT:
    {
      SCROLLINFO si;
      memset(&si, 0, sizeof si);
      si.cbSize = sizeof si;
      si.fMask = SIF_PAGE | SIF_RANGE;
      GetScrollInfo(hWnd, SB_VERT, &si);
      doScroll(scrollPos.x, scrollPos.y + si.nPage < si.nMax ? scrollPos.y + si.nPage : si.nMax);

      POINT pt = toPoint(caret);
      pt.y += si.nPage;
      caret = fromPoint(pt);
      if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000))
        selStart = caret;
      updateCaret();
    }
    break;
  case VK_RETURN:
    {
      POINT pt1 = toPoint(caret);
      if (caret == selStart && !insertMode && caret < getTextLength())
        replace(caret, caret + 1, L"\n");
      else
        replace(selStart, caret, L"\n");
      POINT pt2 = toPoint(caret);
      if (pt2.y > pt1.y)
      {
        int spaces = 0;
        while (lines[pt1.y].text[spaces] == ' ')
          spaces++;
        replace(caret, caret, lines[pt1.y].text.substring(0, spaces), NULL, true);
      }
    }
    break;
  case VK_INSERT:
    insertMode = !insertMode;
    placeCaret();
    break;
  case VK_OEM_4:
  case VK_OEM_6:
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
    {
      int index = -1;
      for (int i = 0; i < focus.length(); i++)
        if (caret >= focus[i].first && caret <= focus[i].first + focus[i].second)
          index = i;
      if (index >= 0)
      {
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
        {
          if (wParam == VK_OEM_6)
          {
            selStart = focus[0].first;
            caret = focus.top().first + focus.top().second;
          }
          else
          {
            caret = focus[0].first;
            selStart = focus.top().first + focus.top().second;
          }
        }
        else
        {
          index = (index + (wParam == VK_OEM_6 ? 1 : -1)) % focus.length();
          if (index < 0) index += focus.length();
          caret = selStart = focus[index].first;
        }
        updateCaret();
      }
    }
    break;
  }
}
