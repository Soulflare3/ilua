#ifndef __FRAMEUI_WINDOW_H__
#define __FRAMEUI_WINDOW_H__

#include <windows.h>

#include "base/types.h"
#include "base/wstring.h"
#include "base/hashmap.h"

class Window
{
  static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  static ATOM windowClass;
  static HashMap<HWND, Window*> handleMap;
  struct TTData;
  TTData* ttData;
  WideString regClass;
protected:
  HWND hWnd;
  WNDPROC origProc;
  virtual uint32 onWndMessage(uint32 message, uint32 wParam, uint32 lParam);
  WNDCLASSEX* createclass(WideString wndClass);
  void create(int x, int y, int width, int height, WideString text, uint32 style, uint32 exStyle,
    HWND parent = NULL);
  void create(WideString wndClass, int x, int y, int width, int height, WideString text, uint32 style,
    uint32 exStyle, HWND parent = NULL);
  void subclass(WideString wndClass, int x, int y, int width, int height, WideString text, uint32 style,
    uint32 exStyle, HWND parent = NULL);

  struct ToolInfo
  {
    RECT rc;
    WideString text;
  };
  virtual int toolHitTest(POINT pt, ToolInfo* ti);
public:
  Window();
  virtual ~Window();

  operator HWND() const
  {
    return hWnd;
  }
  HWND getHandle() const
  {
    return hWnd;
  }

  static Window* fromHandle(HWND hWnd);

  // random functions
  void setText(WideString text);
  WideString getText() const;

  void setFont(HFONT hFont);
  HFONT getFont() const;

  void enable(bool e = true);
  void disable()
  {
    enable(false);
  }
  void showWindow(bool s = true);
  void hideWindow()
  {
    showWindow(false);
  }
  void setRedraw(bool r);

  int id() const;
  void setId(int id);

  void invalidate(bool erase = true);

  void enableTooltips(bool enable = true);

  static WideString getWindowText(HWND hWnd);
};

#endif // __FRAMEUI_WINDOW_H__
