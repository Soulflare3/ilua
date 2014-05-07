#ifndef __FRAMEUI_DRAGDROP__
#define __FRAMEUI_DRAGDROP__

#include "base/types.h"
#include "core/frameui/framewnd.h"
#include <windows.h>

#define WM_DRAGDROP       (WM_USER+283)
#define WM_DRAGOVER       (WM_USER+284)
#define WM_DRAGLEAVE      (WM_USER+285)

class DropTarget : public IDropTarget
{
protected:
  int refCount;
  FORMATETC format;
  uint32 allowedEffects;
  bool allowDrop;
  WindowFrame* window;

  HRESULT __stdcall QueryInterface(REFIID iid, void** ppvObject);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();

  HRESULT __stdcall DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
  HRESULT __stdcall DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
  HRESULT __stdcall DragLeave();
  HRESULT __stdcall Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);

  uint32 DropEffect(uint32 grfKeyState, POINTL pt, uint32 dwAllowed);
public:
  DropTarget(WindowFrame* window, CLIPFORMAT format, uint32 allowedEffects);
  ~DropTarget();
};
class ClipboardReader
{
  IDataObject* pDataObj;
  STGMEDIUM stgmed;
public:
  ClipboardReader(CLIPFORMAT format);
  ~ClipboardReader();

  HGLOBAL getData();
};

uint32 DoDragDrop(CLIPFORMAT format, HGLOBAL data, uint32 allowedEffects, HCURSOR cursor = NULL);
uint32 DoDragDropEx(CLIPFORMAT format, HGLOBAL data, uint32 allowedEffects, HWND hWnd, HCURSOR cursor = NULL);
IDataObject* SetClipboard(CLIPFORMAT format, HGLOBAL data);

char* FileListToString(Array<String> const& files);
HGLOBAL CreateFileDrop(Array<String> const& files);
HGLOBAL CreateFileDrop(String file);

String GetGlobalText(HGLOBAL data);
WideString GetGlobalTextWide(HGLOBAL data);
HGLOBAL CreateGlobalText(String text);
HGLOBAL CreateGlobalText(WideString text);

#endif // __FRAMEUI_DRAGDROP__
