#ifndef __FRAMEUI_FRAMEWND_H__
#define __FRAMEUI_FRAMEWND_H__

#include <windows.h>
#include "core/frameui/frame.h"
#include "core/frameui/window.h"

class WindowFrame : public Frame, public Window
{
  HWND ownerWindow;
protected:
  void onMove(uint32 data);
  uint32 onWndMessage(uint32 message, uint32 wParam, uint32 lParam);
protected:
  void create(WideString text, uint32 style, uint32 exStyle);
  void create(WideString wndClass, WideString text, uint32 style, uint32 exStyle);
  HWND getOwner() const
  {
    return ownerWindow;
  }
public:
  WindowFrame(Frame* parent);
};

class RootWindow : public RootFrame, public Window
{
  uint32 onControlMessage(HWND hControl, uint32 message, uint32 wParam, uint32 lParam);
  uint32 onWndMessage(uint32 message, uint32 wParam, uint32 lParam);

  uint32 beginMoving();
  void endMoving(uint32 data);

  friend class WindowFrame;
  uint32 r_message;
  Frame* r_frame;

  Frame* c_frame;
public:
  RootWindow();
  ~RootWindow();

  static void setCapture(Frame* frame);
};

#endif // __FRAMEUI_FRAMEWND_H__
