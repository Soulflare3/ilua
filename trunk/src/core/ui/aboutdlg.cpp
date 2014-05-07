#include "core/app.h"
#include "aboutdlg.h"
#include "resource.h"

INT_PTR CALLBACK AboutDialog::DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
  case WM_INITDIALOG:
    return TRUE;
  case WM_COMMAND:
    switch (LOWORD(wParam))
    {
    case IDOK:
    case IDCANCEL:
      EndDialog(hDlg, LOWORD(wParam));
      return TRUE;
    }
    break;
  }
  return FALSE;
}
int AboutDialog::run()
{
  return DialogBox(getInstance(), MAKEINTRESOURCE(IDD_ABOUTBOX),
    getApp()->getMainWindow(), DlgProc);
}
