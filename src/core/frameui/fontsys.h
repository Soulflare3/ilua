#ifndef __FRAMEUI_SYSFONT_H__
#define __FRAMEUI_SYSFONT_H__

#include <windows.h>
#include "base/wstring.h"

#define FONT_BOLD       0x0001
#define FONT_ITALIC     0x0002
#define FONT_UNDERLINE  0x0004
#define FONT_STRIKEOUT  0x0008

class FontSys
{
  struct FontStruct
  {
    HFONT font;
    WideString face;
    WideString name;
    int size;
    int flags;
  };
  FontStruct** fonts;
  int numFonts;
  int maxFonts;
  int logPixelsY;
  HDC hDC;
  FontSys();
  static FontSys instance;
  HFONT _getFont(int height, WideString face, int flags = 0);
public:
  ~FontSys();

  static HFONT getSysFont();
  static HFONT getFont(int size, WideString face, int flags = 0);
  static HFONT getFont(int size, int flags = 0);
  static HFONT changeSize(int size, HFONT oldFont = NULL);
  static HFONT changeFlags(int flags, HFONT oldFont = NULL);
  static void setFontName(HFONT hFont, WideString name);
  static HFONT getFontByName(WideString name);

  static HFONT getFont(LOGFONT const& lf);

  static int getFlags(HFONT font = NULL);

  static SIZE getTextSize(HFONT font, WideString text);
  static int getMTextHeight(HFONT font, int width, WideString text);
};

#endif // __FRAMEUI_SYSFONT_H__
