#include "fontsys.h"

FontSys FontSys::instance;

//#define MKSIZE(s)     (-MulDiv(s,instance.logPixelsY,72))
#define MKSIZE(s)     (-(s))

FontSys::FontSys()
{
  LOGFONT lf;
  memset (&lf, 0, sizeof lf);
  lf.lfHeight = -11;
  lf.lfWeight = FW_NORMAL;
  wcscpy(lf.lfFaceName, L"MS Shell Dlg 2");
  lf.lfCharSet = DEFAULT_CHARSET;
  //GetObject (GetStockObject (ANSI_VAR_FONT), sizeof lf, &lf);

  maxFonts = 16;
  numFonts = 1;
  fonts = new FontStruct*[maxFonts];
  fonts[0] = new FontStruct;
  fonts[0]->font = CreateFontIndirect(&lf);
  fonts[0]->face = lf.lfFaceName;
  fonts[0]->size = lf.lfHeight;
  fonts[0]->flags = 0;
  if (lf.lfWidth > FW_NORMAL)
    fonts[0]->flags |= FONT_BOLD;
  if (lf.lfItalic)
    fonts[0]->flags |= FONT_ITALIC;
  if (lf.lfUnderline)
    fonts[0]->flags |= FONT_UNDERLINE;
  if (lf.lfStrikeOut)
    fonts[0]->flags |= FONT_STRIKEOUT;

  hDC = CreateCompatibleDC(NULL);
  logPixelsY = GetDeviceCaps(hDC, LOGPIXELSY);
}
FontSys::~FontSys()
{
  for (int i = 0; i < numFonts; i++)
  {
    DeleteObject(fonts[i]->font);
    delete fonts[i];
  }
  delete[] fonts;
  DeleteDC(hDC);
}

HFONT FontSys::getSysFont()
{
  return instance.fonts[0]->font;
}
HFONT FontSys::_getFont(int height, WideString face, int flags)
{
  int left = 1;
  int right = numFonts;
  while (left < right)
  {
    int mid = (left + right) / 2;
    int compare = fonts[mid]->size - height;
    if (compare == 0)
      compare = fonts[mid]->flags - flags;
    if (compare == 0)
      compare = fonts[mid]->face.compare(face);
    if (compare == 0)
      return fonts[mid]->font;
    if (compare < 0)
      left = mid + 1;
    else
      right = mid;
  }
  if (numFonts >= maxFonts)
  {
    maxFonts *= 2;
    FontStruct** temp = new FontStruct*[maxFonts];
    memcpy(temp, fonts, sizeof(FontStruct*) * numFonts);
    delete[] fonts;
    fonts = temp;
  }
  if (left < numFonts)
    memmove(fonts + left + 1, fonts + left, sizeof(FontStruct*) * (numFonts - left));
  numFonts++;
  fonts[left] = new FontStruct;
  fonts[left]->face = face;
  fonts[left]->size = height;
  fonts[left]->flags = flags;
  LOGFONT lf;
  GetObject(fonts[0]->font, sizeof lf, &lf);
  lf.lfHeight = height;
  wcscpy(lf.lfFaceName, face);
  lf.lfWeight = (flags & FONT_BOLD ? FW_BOLD : FW_NORMAL);
  lf.lfItalic = (flags & FONT_ITALIC ? TRUE : FALSE);
  lf.lfUnderline = (flags & FONT_UNDERLINE ? TRUE : FALSE);
  lf.lfStrikeOut = (flags & FONT_STRIKEOUT ? TRUE : FALSE);
  lf.lfQuality = CLEARTYPE_QUALITY;
  fonts[left]->font = CreateFontIndirect(&lf);
  return fonts[left]->font;
}
HFONT FontSys::getFont(int size, WideString face, int flags)
{
  return instance._getFont(MKSIZE(size), face, flags);
}
HFONT FontSys::getFont(int size, int flags)
{
  return instance._getFont(MKSIZE(size), instance.fonts[0]->face, flags);
}
HFONT FontSys::changeSize(int size, HFONT oldFont)
{
  if (oldFont == NULL) oldFont = getSysFont();
  LOGFONT lf;
  GetObject(oldFont, sizeof lf, &lf);
  int flags = 0;
  if (lf.lfWidth > FW_NORMAL)
    flags |= FONT_BOLD;
  if (lf.lfItalic)
    flags |= FONT_ITALIC;
  if (lf.lfUnderline)
    flags |= FONT_UNDERLINE;
  if (lf.lfStrikeOut)
    flags |= FONT_STRIKEOUT;
  return instance._getFont(MKSIZE (size), lf.lfFaceName, flags);
}
HFONT FontSys::changeFlags (int flags, HFONT oldFont)
{
  if (oldFont == NULL) oldFont = getSysFont();
  LOGFONT lf;
  GetObject(oldFont, sizeof lf, &lf);
  return instance._getFont(lf.lfHeight, lf.lfFaceName, flags);
}

SIZE FontSys::getTextSize(HFONT font, WideString text)
{
  SIZE sz;
  SelectObject(instance.hDC, font);
  GetTextExtentPoint32(instance.hDC, text, text.length(), &sz);
  return sz;
}
int FontSys::getMTextHeight (HFONT font, int width, WideString text)
{
  SIZE sz;
  SelectObject(instance.hDC, font);
  WideString word;
  WideString curline;
  int height = 0;
  for (int i = 0; i <= text.length (); i++)
  {
    if (text[i] && text[i] != '\n' && !s_isspace (text[i]))
      word += text[i];
    if (text[i] == 0 || s_isspace (text[i]))
    {
      WideString tmp = curline + word;
      GetTextExtentPoint32(instance.hDC, tmp, tmp.length(), &sz);
      int wd = sz.cx;
      if (text[i] == '\n' || (wd > width && curline.length ()))
      {
        GetTextExtentPoint32(instance.hDC, curline, curline.length(), &sz);
        height += sz.cy;
        curline = L"";
      }
      curline += word;
      if (text[i])
        curline += text[i];
      word = L"";
    }
  }
  if (curline.length ())
  {
    GetTextExtentPoint32(instance.hDC, curline, curline.length(), &sz);
    height += sz.cy;
  }
  return height;
}

int FontSys::getFlags(HFONT font)
{
  if (font == NULL) font = getSysFont();
  LOGFONT lf;
  GetObject(font, sizeof lf, &lf);
  int flags = 0;
  if (lf.lfWidth > FW_NORMAL)
    flags |= FONT_BOLD;
  if (lf.lfItalic)
    flags |= FONT_ITALIC;
  if (lf.lfUnderline)
    flags |= FONT_UNDERLINE;
  if (lf.lfStrikeOut)
    flags |= FONT_STRIKEOUT;
  return flags;
}

void FontSys::setFontName(HFONT hFont, WideString name)
{
  for (int i = 0; i < instance.numFonts; i++)
  {
    if (instance.fonts[i]->font == hFont)
    {
      instance.fonts[i]->name = name;
      return;
    }
  }
}
HFONT FontSys::getFontByName(WideString name)
{
  for (int i = 0; i < instance.numFonts; i++)
    if (instance.fonts[i]->name == name)
      return instance.fonts[i]->font;
  return instance.fonts[0]->font;
}

HFONT FontSys::getFont(LOGFONT const& lf)
{
  int flags = 0;
  if (lf.lfWidth > FW_NORMAL)
    flags |= FONT_BOLD;
  if (lf.lfItalic)
    flags |= FONT_ITALIC;
  if (lf.lfUnderline)
    flags |= FONT_UNDERLINE;
  if (lf.lfStrikeOut)
    flags |= FONT_STRIKEOUT;
  return instance._getFont(lf.lfHeight, lf.lfFaceName, flags);
}
