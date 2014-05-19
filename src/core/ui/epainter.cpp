#include "editor.h"
#include "base/utils.h"

struct Editor::Painter
{
  Editor* e;
  HDC hDC;
  Painter(Editor* editor, HDC hdc)
    : e(editor)
    , hDC(hdc)
  {
    ox = e->LeftMargin() - e->scrollPos.x * e->chSize.cx;
    activeWnd = (e->hWnd == GetFocus());
  }

  enum {tNull, tKeyword, tNumber, tString, tComment, tFocusKeyword, tFocusBracket};

  bool inFocus(int pos)
  {
    if (e->caret != e->selStart) return false;
    for (int i = 0; i < e->focus.length(); i++)
      if (pos + origin == e->focus[i].first)
        return true;
    return false;
  }

  wchar_t const* str;
  int ctx;
  int ctxType;

  int origin;

  int lexemStart;
  int lexemEnd;
  int lexemType;

  int sela;
  int selb;
  int ox;

  RECT rc;

  bool activeWnd;

  void init(int y, int offset);
  int next();
  void drawchunk(int start, int end, int type);
};
void Editor::Painter::init(int y, int offset)
{
  str = e->lines[y].text.c_str();
  ctx = e->lines[y].ctx;
  ctxType = e->lines[y].ctxType;
  lexemStart = lexemEnd = lexemType = 0;

  origin = offset;

  rc.top = (y - e->scrollPos.y) * e->chSize.cy;
  rc.bottom = rc.top + e->chSize.cy;

  sela = (e->selStart < e->caret ? e->selStart : e->caret) - offset;
  selb = (e->selStart < e->caret ? e->caret : e->selStart) - offset;
}
int Editor::Painter::next()
{
  int pos = lexemEnd;
  if (str[pos] == 0)
  {
    lexemStart = pos;
    lexemType = tNull;
    return lexemType;
  }

  if (ctx)
  {
    lexemStart = 0;
    if (ctx == ctxString)
      lexemType = tString;
    else
      lexemType = tComment;
    while (str[pos])
    {
      if (str[pos] == ']')
      {
        pos++;
        int count = 0;
        while (str[pos] == '=' && count < ctxType)
          pos++, count++;
        if (str[pos] == ']' && count == ctxType)
        {
          lexemEnd = pos + 1;
          ctx = 0;
          ctxType = 0;
          return lexemType;
        }
      }
      else
        pos++;
    }
    lexemEnd = pos;
    return lexemType;
  }

  while (str[pos])
  {
    if (str[pos] == '-' && str[pos + 1] == '-')
    {
      lexemStart = pos;
      lexemType = tComment;
      pos += 2;
      int ctxCount = -1;
      if (str[pos] == '[')
      {
        pos++;
        ctxCount = 0;
        while (str[pos] == '=')
          pos++, ctxCount++;
        if (str[pos] != '[')
          ctxCount = -1;
      }
      while (str[pos])
      {
        if (ctxCount >= 0 && str[pos] == ']')
        {
          pos++;
          int count = 0;
          while (str[pos] == '=' && count < ctxCount)
            pos++, count++;
          if (str[pos] == ']' && count == ctxCount)
          {
            lexemEnd = pos + 1;
            return lexemType;
          }
        }
        else
          pos++;
      }
      lexemEnd = pos;
      return lexemType;
    } // end comment
    if (str[pos] == '"' || str[pos] == '\'')
    {
      lexemStart = pos++;
      lexemType = tString;
      while (str[pos])
      {
        if (str[pos] == '\\')
          pos++;
        else if (str[pos] == str[lexemStart])
        {
          pos++;
          break;
        }
        if (str[pos])
          pos++;
      }
      lexemEnd = pos;
      return lexemType;
    } // end quoted string
    if (str[pos] == '[')
    {
      lexemType = tString;
      lexemStart = pos++;
      int ctxCount = 0;
      while (str[pos] == '=')
        pos++, ctxCount++;
      if (str[pos] == '[')
      {
        while (str[pos])
        {
          if (str[pos] == ']')
          {
            pos++;
            int count = 0;
            while (str[pos] == '=' && count < ctxCount)
              pos++, count++;
            if (str[pos] == ']' && count == ctxCount)
            {
              lexemEnd = pos + 1;
              return lexemType;
            }
          }
          else
            pos++;
        }
        lexemEnd = pos;
        return lexemType;
      }
      else if (inFocus(lexemStart))
      {
        lexemEnd = lexemStart + 1;
        return lexemType = tFocusBracket;
      }
      else
        pos = lexemStart;
    } // end long string
    if ((str[pos] >= '0' && str[pos] <= '9') ||
        (str[pos] == '.' && str[pos + 1] >= '0' && str[pos + 1] <= '9'))
    {
      lexemType = tNumber;
      lexemStart = pos;
      bool expo = false;
      if (str[pos] == '0' && (str[pos + 1] == 'x' || str[pos + 1] == 'X'))
      {
        pos += 2;
        while ((str[pos] >= '0' && str[pos] <= '9') ||
               (str[pos] >= 'a' && str[pos] <= 'f') ||
               (str[pos] >= 'A' && str[pos] <= 'F'))
          pos++;
        if (str[pos] == '.')
        {
          pos++;
          while ((str[pos] >= '0' && str[pos] <= '9') ||
                 (str[pos] >= 'a' && str[pos] <= 'f') ||
                 (str[pos] >= 'A' && str[pos] <= 'F'))
            pos++;
        }
        if (str[pos] == 'p' || str[pos] == 'P')
          expo = true;
      }
      else
      {
        while (str[pos] >= '0' && str[pos] <= '9')
          pos++;
        if (str[pos] == '.')
        {
          pos++;
          while (str[pos] >= '0' && str[pos] <= '9')
            pos++;
        }
        if (str[pos] == 'e' || str[pos] == 'E')
          expo = true;
      }
      if (expo)
      {
        pos++;
        if ((str[pos] == '+' || str[pos] == '-') && str[pos + 1] >= '0' && str[pos + 1] <= '9')
          pos++;
        while (str[pos] >= '0' && str[pos] <= '9')
          pos++;
      }
      lexemEnd = pos;
      return lexemType;
    } // end number
    if ((str[pos] >= 'a' && str[pos] <= 'z') ||
        (str[pos] >= 'A' && str[pos] <= 'Z') || str[pos] == '_')
    {
      lexemType = tNull;
      lexemStart = pos;
      while ((str[pos] >= 'a' && str[pos] <= 'z') ||
             (str[pos] >= 'A' && str[pos] <= 'Z') ||
             (str[pos] >= '0' && str[pos] <= '9') || str[pos] == '_')
        pos++;
      lexemEnd = pos;
      WideString key(str + lexemStart, lexemEnd - lexemStart);
      if (e->keywords.has(key))
        lexemType = e->keywords.get(key);
      if (lexemType != tNull)
      {
        if (inFocus(lexemStart))
          lexemType = tFocusKeyword;
        return lexemType;
      }
    } // end literal
    else if ((str[pos] == '(' || str[pos] == ')' || str[pos] == '{' || str[pos] == '}' || str[pos] == ']') && inFocus(pos))
    {
      lexemStart = pos;
      lexemEnd = pos + 1;
      return lexemType = tFocusBracket;
    }
    else
      pos++;
  }
  lexemStart = pos;
  lexemEnd = pos;
  lexemType = tNull;
  return lexemType;
}
void Editor::Painter::drawchunk(int start, int theend, int type)
{
  while (start < theend)
  {
    int end = theend;
    if (sela > start && sela < end)
      end = sela;
    else if (selb > start && selb < end)
      end = selb;

    if (start >= sela && start < selb)
    {
      if (activeWnd)
        SetBkColor(hDC, e->settings->selection.activebg);
      else
        SetBkColor(hDC, e->settings->selection.inactivebg);
    }
    else if (activeWnd && (type == tFocusBracket || type == tFocusKeyword))
      SetBkColor(hDC, e->settings->colors.focusbg);
    else
      SetBkColor(hDC, e->settings->bgcolor);
    if (type == 0 || type == tFocusBracket)
      SetTextColor(hDC, e->settings->colors.text);
    else if (type == tKeyword || type == tFocusKeyword)
      SetTextColor(hDC, e->settings->colors.keyword);
    else if (type == tNumber)
      SetTextColor(hDC, e->settings->colors.text);
    else if (type == tString)
      SetTextColor(hDC, e->settings->colors.string);
    else if (type == tComment)
      SetTextColor(hDC, e->settings->colors.comment);
    rc.left = ox + start * e->chSize.cx;
    rc.right = ox + end * e->chSize.cx;
    DrawText(hDC, str + start, end - start, &rc, DT_NOPREFIX);
    start = end;
  }
}

void Editor::paint()
{
  PAINTSTRUCT ps;
  HDC hPaintDC = BeginPaint(hWnd, &ps);

  if (hDC == NULL)
    hDC = CreateCompatibleDC(hPaintDC);
  if (hBitmap == NULL)
  {
    RECT rc;
    GetClientRect(hWnd, &rc);
    if (rc.right < 10) rc.right = 10;
    if (rc.bottom < 10) rc.bottom = 10;
    hBitmap = CreateCompatibleBitmap(hPaintDC, rc.right, rc.bottom);
    SelectObject(hDC, hBitmap);
  }

  Painter p(this, hDC);

  SetBkMode(hDC, OPAQUE);

  bool activeWnd = (GetFocus() == hWnd);

  if (settings->bpOffset)
  {
    SetBkColor(hDC, 0xF0F0F0);
    p.rc.top = ps.rcPaint.top;
    p.rc.bottom = ps.rcPaint.bottom;
    p.rc.left = -scrollPos.x * chSize.cx;
    p.rc.right = p.rc.left + settings->bpOffset;
    ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &p.rc, NULL, 0, NULL);
  }

  p.rc.top = ps.rcPaint.top;
  p.rc.bottom = p.rc.top;
  int absOffset = 0;
  for (int y = 0; y < lines.length(); y++)
  {
    p.init(y, absOffset);
    if (p.rc.top > ps.rcPaint.bottom)
      break;

    if (p.rc.bottom >= ps.rcPaint.top)
    {
      SelectObject(hDC, hFont);

      SetBkColor(hDC, settings->bgcolor);
      if (settings->lineNumbers.margin + settings->lineNumbers.fields)
      {
        p.rc.left = settings->bpOffset - scrollPos.x * chSize.cx;
        p.rc.right = p.rc.left + LeftMargin();
        ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &p.rc, NULL, 0, NULL);
      }
      if (settings->lineNumbers.fields)
      {
        p.rc.left = settings->bpOffset - scrollPos.x * chSize.cx;
        p.rc.right = settings->bpOffset + (settings->lineNumbers.fields - scrollPos.x) * chSize.cx;
        WideString lnum;
        if (settings->mode == 0)
          lnum = WideString(y + 1);
        else if (lines[y].timestamp)
          lnum = WideString(format_systime(lines[y].timestamp, "%H:%M:%S"));
        SetTextColor(hDC, settings->colors.line);
        DrawText(hDC, lnum.c_str(), lnum.length(), &p.rc, DT_RIGHT);
      }

      if (settings->mode == 0)
      {
        int prev = 0;
        do
        {
          p.next();
          SelectObject(hDC, hFont);
          if (p.lexemStart > prev)
            p.drawchunk(prev, p.lexemStart, 0);
          if (p.lexemStart < p.lexemEnd)
          {
            //if (p.lexemType == Painter::tFocusBracket || p.lexemType == Painter::tFocusKeyword)
            //  SelectObject(hDC, hFontBf);
            p.drawchunk(p.lexemStart, p.lexemEnd, p.lexemType);
          }
          prev = p.lexemEnd;
        } while (p.lexemType);
      }
      else
      {
        if (lines[y].ctx == Painter::tKeyword)
          SelectObject(hDC, hFontIt);
        p.drawchunk(0, lines[y].text.length(), lines[y].ctx);
      }

      if (p.rc.right < ps.rcPaint.right)
      {
        SetBkColor(hDC, settings->bgcolor);
        p.rc.left = p.rc.right;
        p.rc.right = ps.rcPaint.right;
        ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &p.rc, NULL, 0, NULL);
      }

      if (settings->mode == 0)
      {
        if (lines[y].breakpoint > 0)
          DrawIconEx(hDC, -scrollPos.x * chSize.cx, p.rc.top, icons[iBreakpoint],
            16, 16, 0, NULL, DI_IMAGE | DI_MASK);
        if (y == currentLine)
          DrawIconEx(hDC, -scrollPos.x * chSize.cx, p.rc.top, icons[iCurline],
            16, 16, 0, NULL, DI_IMAGE | DI_MASK);
      }
    }

    absOffset += lines[y].text.length() + 1;
  }

  if (p.rc.bottom < ps.rcPaint.bottom)
  {
    SetBkColor(hDC, settings->bgcolor);
    p.rc.top = p.rc.bottom;
    p.rc.bottom = ps.rcPaint.bottom;
    p.rc.left = settings->bpOffset - scrollPos.x * chSize.cx;
    p.rc.right = ps.rcPaint.right;
    ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &p.rc, NULL, 0, NULL);
  }

  BitBlt(hPaintDC, ps.rcPaint.left, ps.rcPaint.top,
    ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
    hDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);

  EndPaint(hWnd, &ps);
}
