#ifndef __FRAMEUI_LISTCTRL__
#define __FRAMEUI_LISTCTRL__

#include "core/frameui/frame.h"
#include "core/frameui/window.h"
#include "core/frameui/framewnd.h"

class ListFrame : public WindowFrame
{
  HIMAGELIST iList;
  HashMap<int, int> iLoaded;
  int getIcon(int id);
public:
  ListFrame(Frame* parent, int id = 0,
    int style = LVS_ALIGNLEFT | LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL,
    int styleEx = LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
  ~ListFrame();

  void clear();
  void clearColumns();

  int getItemParam(int item);

  int getCount() const;
  void insertColumn(int i, WideString name, int width = 0, int fmt = LVCFMT_LEFT);
  void setColumnWidth(int i, int width);
  int addItem(WideString name, int image, int data);
  void setItemText(int item, int column, WideString text);
  void setItemImage(int item, int img);

  int getCurSel() const
  {
    return ListView_GetNextItem(hWnd, -1, LVNI_SELECTED);
  }
};

#endif // __FRAMEUI_LISTCTRL__
