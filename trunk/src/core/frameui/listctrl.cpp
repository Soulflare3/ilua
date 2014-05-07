#include "core/app.h"

#include "core/frameui/framewnd.h"
#include "core/frameui/fontsys.h"

#include "listctrl.h"
#include "resource.h"

int ListFrame::getIcon(int id)
{
  if (iLoaded.has(id))
    return iLoaded.get(id);
  HICON hIcon = (HICON) LoadImage(getInstance(), MAKEINTRESOURCE(id), IMAGE_ICON, 16, 16, 0);
  int index = ImageList_AddIcon(iList, hIcon);
  DestroyIcon(hIcon);
  iLoaded.set(id, index);
  return index;
}
ListFrame::ListFrame(Frame* parent, int id, int style, int styleEx)
  : WindowFrame(parent)
{
  iList = ImageList_Create(16, 16, ILC_COLOR24 | ILC_MASK, 4, 4);
  create(WC_LISTVIEW, L"", WS_CHILD | WS_TABSTOP | style, WS_EX_CLIENTEDGE);
  setFont(FontSys::getSysFont());
  setId(id);
  ListView_SetExtendedListViewStyle(hWnd, styleEx);
  ListView_SetImageList(hWnd, iList, LVSIL_SMALL);
}
ListFrame::~ListFrame()
{
  ImageList_Destroy(iList);
}

void ListFrame::clear()
{
  ListView_DeleteAllItems(hWnd);
}
void ListFrame::clearColumns()
{
  for (int i = Header_GetItemCount(ListView_GetHeader(hWnd)) - 1; i >= 0; i--)
    ListView_DeleteColumn(hWnd, i);
}

void ListFrame::insertColumn(int i, WideString name, int width, int fmt)
{
  LVCOLUMN lvc;
  memset(&lvc, 0, sizeof lvc);
  lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
  lvc.fmt = fmt;
  lvc.cx = width;
  lvc.pszText = name.getBuffer();
  ListView_InsertColumn(hWnd, i, &lvc);
}
void ListFrame::setColumnWidth(int i, int width)
{
  ListView_SetColumnWidth(hWnd, i, width);
}
int ListFrame::addItem(WideString name, int image, int data)
{
  LVITEM lvi;
  memset(&lvi, 0, sizeof lvi);
  lvi.iItem = ListView_GetItemCount(hWnd);
  lvi.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
  lvi.pszText = name.getBuffer();
  lvi.iImage = getIcon(image ? image : IDI_BLANK);
  lvi.lParam = data;
  ListView_InsertItem(hWnd, &lvi);
  return lvi.iItem;
}
void ListFrame::setItemText(int item, int column, WideString text)
{
  LVITEM lvi;
  memset(&lvi, 0, sizeof lvi);
  lvi.iItem = item;
  lvi.iSubItem = column;
  lvi.mask = LVIF_TEXT;
  lvi.pszText = text.getBuffer();
  ListView_SetItem(hWnd, &lvi);
}
void ListFrame::setItemImage(int item, int image)
{
  LVITEM lvi;
  memset(&lvi, 0, sizeof lvi);
  lvi.iItem = item;
  lvi.mask = LVIF_IMAGE;
  lvi.iImage = getIcon(image ? image : IDI_BLANK);
  ListView_SetItem(hWnd, &lvi);
}
int ListFrame::getCount() const
{
  return ListView_GetItemCount(hWnd);
}
int ListFrame::getItemParam(int item)
{
  LVITEM lvi;
  memset(&lvi, 0, sizeof lvi);
  lvi.iItem = item;
  lvi.mask = LVIF_PARAM;
  ListView_GetItem(hWnd, &lvi);
  return lvi.lParam;
}
