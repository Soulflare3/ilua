#ifndef __UI_EDITOR__
#define __UI_EDITOR__

#include "core/frameui/framewnd.h"
#include "core/frameui/dragdrop.h"
#include "base/array.h"
#include "base/string.h"
#include "base/dictionary.h"
#include "base/hashmap.h"

class MainWnd;

struct EditorSettings
{
  struct LineNumbers
  {
    uint32 fields;
    uint32 bgcolor;
    uint32 margin;
  } lineNumbers;
  struct Font
  {
    uint32 size;
    uint32 flags;
  } font;
  uint32 bgcolor;
  struct Colors
  {
    uint32 line;
    uint32 text;
    uint32 number;
    uint32 keyword;
    uint32 string;
    uint32 comment;
    uint32 focusbg;
  } colors;
  struct Selection
  {
    uint32 activebg;
    uint32 inactivebg;
  } selection;

  uint32 bpOffset;
  uint32 tabSize;
  uint32 mode;

  static EditorSettings defaultSettings;
  static EditorSettings logSettings;
};

#define EN_SETBREAKPOINT    (WM_USER+115)
#define EN_DBGCOMMAND       (WM_USER+173)
#define EN_MODIFIED         (WM_USER+174)
#define EN_FOCUSED          (WM_USER+175)

class Editor : public WindowFrame
{
  enum {ctxNone, ctxString, ctxComment};
  enum {cArrow, cBeam, cCount};
  enum {iBreakpoint, iCurline, iCount};
  HCURSOR cursors[cCount];
  HICON icons[iCount];
  EditorSettings* settings;
  HFONT hFont;
  HFONT hFontIt;
  HFONT hFontBf;
  SIZE chSize;
  int LeftMargin()
  {
    return settings->bpOffset + settings->lineNumbers.fields * chSize.cx + settings->lineNumbers.margin;
  }

  MainWnd* running;

  struct Line
  {
    WideString text;
    int ctx;
    int endComment;
    union
    {
      struct
      {
        int ctxType;
        int breakpoint;
      };
      uint64 timestamp;
    };
    Line()
      : ctx(0), ctxType(0), breakpoint(0), timestamp(0), endComment(-1)
    {}
    Line(WideString str, int c = 0, int ct = 0)
      : text(str), ctx(c), ctxType(ct)
    {}
  };
  Array<Line> lines;

  typedef Pair<int, int> FocusWord;
  Array<FocusWord> focus;

  IDataObject* pCopyLine;

  HashMap<WideString, int> keywords;
  HashMap<WideString, int> kwInGraph;
  HashMap<WideString, int> kwGraph;
  HashMap<WideString, int> kwBegin;
  HashMap<WideString, int> kwEnd;
  void addKwGraph(wchar_t const* a, wchar_t const* b)
  {
    kwInGraph.set(a, 1);
    kwInGraph.set(b, 1);
    kwGraph.set(WideString::format(L"%s+%s", a, b), 1);
  }
  bool inKwGraph(wchar_t const* a, wchar_t const* b)
  {
    return kwGraph.has(WideString::format(L"%s+%s", a, b));
  }
  struct Painter;
  friend struct Painter;
  void paint();
  void initkw();

  int caretX;
  int caret;
  int selStart;
  POINT scrollPos;
  POINT scrollAccum;
  POINT extent;
  bool insertMode;
  DropTarget* target;
  int dragop;
  int dropPos;
  int currentLine;

  WideString getSelection();
  WideString substring(int a, int b);

  struct HistoryItem
  {
    bool glue;
    int begin;
    int end;
    WideString text;
  };
  Array<HistoryItem> history;
  int historyPos;
  int origHistory;
  int replace(int ibegin, int iend, WideString str, HistoryItem* mod = NULL, bool glue = false);

  void fixPoint(POINT& pt);
  POINT toPoint(int offset);
  int fromPoint(POINT pt);
  POINT paramToPoint(uint32 param);

  void updateExtent();
  void updateCaret();
  void updateFocus();
  void placeCaret();
  void doScroll(int x, int y);

  WideString nextKey(POINT& pt, bool bracket);
  WideString prevKey(POINT& pt, bool bracket);

  int updateCtx(int from, int to);
  int wordEnd(wchar_t const* str, int pos, int dir);
  int getWordSize(int pos, int dir);
  void untabify(WideString& text);
  void onKey(uint32 wParam);
  HashMap<int, bool>* breakpoints;
  void toggleBreakpoint(int y);

  HDC hDC;
  HBITMAP hBitmap;
  int toolHitTest(POINT pt, ToolInfo* ti);
  uint32 onMessage(uint32 message, uint32 wParam, uint32 lParam);
public:
  Editor(Frame* parent, EditorSettings* settings = &EditorSettings::defaultSettings, int id = 0);
  ~Editor();

  void setCursor(int line, int col);
  void selectLine(int line);

  bool tryClose(char const* path, wchar_t const* title);

  void setRunning(MainWnd* e)
  {
    running = e;
    getParent()->notify(EN_MODIFIED, (uint32) this, 0);
  }
  bool isRunning() const
  {
    return running != NULL;
  }

  int getTextLength(bool crlf = false) const;

  bool modified() const
  {
    return (origHistory != historyPos);
  }
  void setModified(bool mod)
  {
    if (mod)
      origHistory = -1;
    else
      origHistory = historyPos;
    getParent()->notify(EN_MODIFIED, (uint32) this, 0);
  }

  void load(wchar_t const* name);
  void save(wchar_t const* name);

  int getNumLines() const
  {
    return lines.length();
  }
  bool hasBreakpoint(int line)
  {
    return (line >= 0 && line < lines.length() && lines[line].breakpoint > 0);
  }
  void setCurrentLine(int line)
  {
    currentLine = line;
    invalidate();
  }
  void setBreakLines(Array<int>& bp);

  void clear();

  void addLogMessage(char const* text, uint64 timestamp, uint32 type);
};

#endif // __UI_EDITOR__
