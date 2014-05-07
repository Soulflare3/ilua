#ifndef __UI_ABOUTDLG__
#define __UI_ABOUTDLG__

class AboutDialog
{
  static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
public:
  static int run();
};

#endif // __UI_ABOUTDLG__
