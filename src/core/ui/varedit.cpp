#include "varlist.h"
#include "core/frameui/fontsys.h"

class VarList::LabelEditor : public Window
{
  VarList* list;
  uint32 onWndMessage(uint32 message, uint32 wParam, uint32 lParam);
public:
  VarList::VarData* node;
  bool key;
  LabelEditor(VarList* lst);
};
VarList::LabelEditor::LabelEditor(VarList* lst)
  : list(lst)
  , node(NULL)
  , key(false)
{
  subclass(L"Edit", 0, 0, 10, 10, L"", ES_AUTOHSCROLL | WS_CHILD, 0, lst->getHandle());
  setFont(FontSys::getFont(12, L"Courier New"));
  hideWindow();
}
uint32 VarList::LabelEditor::onWndMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  if (message == WM_KILLFOCUS && node != NULL)
  {
    VarData* n = node;
    node = NULL;
    list->finishEdit(n, key, getText());
    list->cancelEdit();
  }
  else if (message == WM_CHAR && node != NULL)
  {
    if (wParam == VK_RETURN)
    {
      VarData* n = node;
      bool k = key;
      node = NULL;
      if (list->finishEdit(n, k, getText()))
      {
        list->cancelEdit();
        return 0;
      }
      else
      {
        node = n;
        key = k;
      }
    }
    else if (wParam == VK_ESCAPE)
    {
      list->cancelEdit();
      return 0;
    }
  }
  return Window::onWndMessage(message, wParam, lParam);
}

void VarList::editLabel(VarData* node, bool key, uint32 delay)
{
  if (editor == NULL)
    editor = new LabelEditor(this);
  editor->hideWindow();

  editor->node = node;
  editor->key = key;
  if (delay == 0)
    startEdit();
  else
    SetTimer(hWnd, 0, delay, NULL);
}
void VarList::startEdit()
{
  if (editor && editor->node)
  {
    selected = NULL;
    invalidate();
    RECT rc = getNodeRect(editor->node, editor->key ? htKey : htValue);
    rc.top++;
    rc.right--;
    SetWindowPos(editor->getHandle(), NULL, rc.left, rc.top, rc.right - rc.left,
      rc.bottom - rc.top, SWP_NOZORDER);
    editor->setText(getEditInit(editor->node, editor->key));
    PostMessage(editor->getHandle(), EM_SETSEL, 0, -1);
    editor->showWindow();
    SetFocus(editor->getHandle());
  }
}
void VarList::cancelEdit()
{
  if (editor)
  {
    if (editor->node)
      select(editor->node);
    editor->node = NULL;
    editor->hideWindow();
  }
}
void VarList::delEdit()
{
  delete editor;
  editor = NULL;
}

void VarList::onchanged()
{
  getParent()->notify(WM_COMMAND, id() | (VN_VARCHANGED << 16), (uint32) hWnd);
}
