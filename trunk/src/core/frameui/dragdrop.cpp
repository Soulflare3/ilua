#include "dragdrop.h"
#include <shlobj.h>

////////////////////// DropTarget ///////////////////////////////////////

HRESULT __stdcall DropTarget::QueryInterface(REFIID iid, void** ppvObject)
{
  if (iid == IID_IDropTarget || iid == IID_IUnknown)
  {
    AddRef();
    *ppvObject = this;
    return S_OK;
  }
  else
  {
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
}
ULONG __stdcall DropTarget::AddRef()
{
  return InterlockedIncrement((LONG*) &refCount);
}
ULONG __stdcall DropTarget::Release()
{
  int count = InterlockedDecrement((LONG*) &refCount);
  if (count == 0)
    delete this;
  return count;
}

uint32 DropTarget::DropEffect(uint32 grfKeyState, POINTL pt, uint32 dwAllowed)
{
  uint32 dwEffect = 0;
  dwAllowed &= allowedEffects;
  if (grfKeyState & MK_CONTROL)
    dwEffect = dwAllowed & DROPEFFECT_COPY;
  else if (grfKeyState & MK_SHIFT)
    dwEffect = dwAllowed & DROPEFFECT_MOVE;
  if (dwEffect == 0)
  {
    if (dwAllowed & DROPEFFECT_COPY) dwEffect = DROPEFFECT_COPY;
    if (dwAllowed & DROPEFFECT_MOVE) dwEffect = DROPEFFECT_MOVE;
  }
  return dwEffect;
}
HRESULT __stdcall DropTarget::DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
  allowDrop = (pDataObj->QueryGetData(&format) == S_OK);
  if (allowDrop)
  {
    *pdwEffect = DropEffect(grfKeyState, pt, *pdwEffect);
    SetFocus(window->getHandle());
    ScreenToClient(window->getHandle(), (POINT*) &pt);
    window->notify(WM_DRAGOVER, (uint32) pdwEffect, MAKELONG(pt.x, pt.y));
  }
  else
    *pdwEffect = DROPEFFECT_NONE;
  return S_OK;
}
HRESULT __stdcall DropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
  if (allowDrop)
  {
    *pdwEffect = DropEffect(grfKeyState, pt, *pdwEffect);
    ScreenToClient(window->getHandle(), (POINT*) &pt);
    if (window->notify(WM_DRAGOVER, (uint32) pdwEffect, MAKELONG(pt.x, pt.y)))
      *pdwEffect = DROPEFFECT_NONE;
  }
  else
    *pdwEffect = DROPEFFECT_NONE;
  return S_OK;
}
HRESULT __stdcall DropTarget::DragLeave()
{
  if (allowDrop)
    window->notify(WM_DRAGLEAVE, 0, 0);
  return S_OK;
}
HRESULT __stdcall DropTarget::Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
  if (allowDrop)
  {
    *pdwEffect = DropEffect(grfKeyState, pt, *pdwEffect);
    ScreenToClient(window->getHandle(), (POINT*) &pt);
    window->notify(WM_DRAGOVER, grfKeyState, MAKELONG(pt.x, pt.y));
    STGMEDIUM stgmed;
    if (pDataObj->GetData(&format, &stgmed) == S_OK)
    {
      *pdwEffect = window->notify(WM_DRAGDROP, (uint32) stgmed.hGlobal, *pdwEffect);
      ReleaseStgMedium(&stgmed);
    }
    window->notify(WM_DRAGLEAVE, 0, 0);
    allowDrop = false;
  }
  else
    *pdwEffect = DROPEFFECT_NONE;
  return S_OK;
}

DropTarget::DropTarget(WindowFrame* _window, CLIPFORMAT _format, uint32 _allowedEffects)
{
  refCount = 1;
  window = _window;
  format.cfFormat = _format;
  format.ptd = NULL;
  format.dwAspect = DVASPECT_CONTENT;
  format.lindex = -1;
  format.tymed = TYMED_HGLOBAL;
  allowedEffects = _allowedEffects;
  allowDrop = false;
  RegisterDragDrop(window->getHandle(), this);
}
DropTarget::~DropTarget()
{
  RevokeDragDrop(window->getHandle());
}

/////////////////////// EnumFormatEtc ///////////////////////////////////

class EnumFormatEtc : public IEnumFORMATETC
{
public:
  HRESULT __stdcall QueryInterface(REFIID iid, void** ppvObject);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();

  HRESULT __stdcall Next(ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched);
  HRESULT __stdcall Skip(ULONG celt);
  HRESULT __stdcall Reset();
  HRESULT __stdcall Clone(IEnumFORMATETC** ppEnumFormatEtc);

  EnumFormatEtc(uint32 numFormats, FORMATETC* pFormatEtc);
  ~EnumFormatEtc();

private:
  int refCount;
  uint32 index;
  uint32 count;
  FORMATETC* fmtetc;
};

HRESULT __stdcall EnumFormatEtc::QueryInterface(REFIID iid, void** ppvObject)
{
  if (iid == IID_IEnumFORMATETC || iid == IID_IUnknown)
  {
    AddRef();
    *ppvObject = this;
    return S_OK;
  }
  else
  {
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
}
ULONG __stdcall EnumFormatEtc::AddRef()
{
  return InterlockedIncrement((LONG*) &refCount);
}
ULONG __stdcall EnumFormatEtc::Release()
{
  int count = InterlockedDecrement((LONG*) &refCount);
  if (count == 0)
    delete this;
  return count;
}

HRESULT __stdcall EnumFormatEtc::Next(ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched)
{
  if (celt == 0 || rgelt == NULL)
    return E_INVALIDARG;
  uint32 copied = 0;
  while (index < count && copied < celt)
  {
    rgelt[copied] = fmtetc[index];
    if (rgelt[copied].ptd)
    {
      rgelt[copied].ptd = (DVTARGETDEVICE*) CoTaskMemAlloc(sizeof(DVTARGETDEVICE));
      *rgelt[copied].ptd = *fmtetc[index].ptd;
    }
    index++;
    copied++;
  }
  if (pceltFetched)
    *pceltFetched = copied;
  return (copied == celt ? S_OK : S_FALSE);
}
HRESULT __stdcall EnumFormatEtc::Skip(ULONG celt)
{
  index += celt;
  return (index <= count ? S_OK : S_FALSE);
}
HRESULT __stdcall EnumFormatEtc::Reset()
{
  index = 0;
  return S_OK;
}
HRESULT __stdcall EnumFormatEtc::Clone(IEnumFORMATETC** ppEnumFormatEtc)
{
  EnumFormatEtc* clone = new EnumFormatEtc(count, fmtetc);
  clone->index = index;
  *ppEnumFormatEtc = clone;
  return S_OK;
}

EnumFormatEtc::EnumFormatEtc(uint32 numFormats, FORMATETC* pFormatEtc)
{
  refCount = 1;
  index = 0;
  count = numFormats;
  if (count)
  {
    fmtetc = new FORMATETC[count];
    for (uint32 i = 0; i < count; i++)
    {
      fmtetc[i] = pFormatEtc[i];
      if (fmtetc[i].ptd)
      {
        fmtetc[i].ptd = (DVTARGETDEVICE*) CoTaskMemAlloc(sizeof(DVTARGETDEVICE));
        *fmtetc[i].ptd = *pFormatEtc[i].ptd;
      }
    }
  }
  else
    fmtetc = NULL;
}
EnumFormatEtc::~EnumFormatEtc()
{
  for (uint32 i = 0; i < count; i++)
    if (fmtetc[i].ptd)
      CoTaskMemFree(fmtetc[i].ptd);
  delete[] fmtetc;
}

/////////////////////// DataObject //////////////////////////////////////

class DataObject : public IDataObject
{
public:
  HRESULT __stdcall QueryInterface(REFIID iid, void** ppvObject);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();

  HRESULT __stdcall GetData(FORMATETC* pFormatEtc, STGMEDIUM* pMedium);
  HRESULT __stdcall GetDataHere(FORMATETC* pFormatEtc, STGMEDIUM* pMedium);
  HRESULT __stdcall QueryGetData(FORMATETC* pFormatEtc);
  HRESULT __stdcall GetCanonicalFormatEtc(FORMATETC* pFormatEtc, FORMATETC* pFormatEtcOut);
  HRESULT __stdcall SetData(FORMATETC* pFormatEtc, STGMEDIUM* pMedium, BOOL fRelease);
  HRESULT __stdcall EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppEnumFormatEtc);
  HRESULT __stdcall DAdvise(FORMATETC* pFormatEtc, DWORD advf, IAdviseSink* pAdvSink, DWORD* pdwConnection);
  HRESULT __stdcall DUnadvise(DWORD dwConnection);
  HRESULT __stdcall EnumDAdvise(IEnumSTATDATA** ppEnumAdvise);

  DataObject(CLIPFORMAT format, HGLOBAL data);
  ~DataObject();

private:
  int refCount;
  FORMATETC fmtetc;
  STGMEDIUM stgmed;
};

HRESULT __stdcall DataObject::QueryInterface(REFIID iid, void** ppvObject)
{
  if (iid == IID_IDataObject || iid == IID_IUnknown)
  {
    AddRef();
    *ppvObject = this;
    return S_OK;
  }
  else
  {
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
}
ULONG __stdcall DataObject::AddRef()
{
  return InterlockedIncrement((LONG*) &refCount);
}
ULONG __stdcall DataObject::Release()
{
  int count = InterlockedDecrement((LONG*) &refCount);
  if (count == 0)
    delete this;
  return count;
}

HRESULT __stdcall DataObject::GetData(FORMATETC* pFormatEtc, STGMEDIUM* pMedium)
{
  if (fmtetc.cfFormat != pFormatEtc->cfFormat || fmtetc.dwAspect != pFormatEtc->dwAspect ||
      (fmtetc.tymed & pFormatEtc->tymed) == 0)
    return DV_E_FORMATETC;
  pMedium->tymed = fmtetc.tymed;
  pMedium->pUnkForRelease = NULL;
  if (fmtetc.tymed == TYMED_HGLOBAL)
  {
    uint32 len = GlobalSize(stgmed.hGlobal);
    void* source = GlobalLock(stgmed.hGlobal);
    pMedium->hGlobal = GlobalAlloc(GMEM_FIXED, len);
    memcpy((void*) pMedium->hGlobal, source, len);
    GlobalUnlock(stgmed.hGlobal);
    return S_OK;
  }
  else
    return DV_E_FORMATETC;
}
HRESULT __stdcall DataObject::GetDataHere(FORMATETC* pFormatEtc, STGMEDIUM* pMedium)
{
  return DV_E_FORMATETC;
}
HRESULT __stdcall DataObject::QueryGetData(FORMATETC* pFormatEtc)
{
  if (fmtetc.cfFormat != pFormatEtc->cfFormat || fmtetc.dwAspect != pFormatEtc->dwAspect ||
      (fmtetc.tymed & pFormatEtc->tymed) == 0)
    return DV_E_FORMATETC;
  return S_OK;
}
HRESULT __stdcall DataObject::GetCanonicalFormatEtc(FORMATETC* pFormatEtc, FORMATETC* pFormatEtcOut)
{
  pFormatEtcOut->ptd = NULL;
  return E_NOTIMPL;
}
HRESULT __stdcall DataObject::SetData(FORMATETC* pFormatEtc, STGMEDIUM* pMedium, BOOL fRelease)
{
  return E_NOTIMPL;
}
HRESULT __stdcall DataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppEnumFormatEtc)
{
  if (dwDirection == DATADIR_GET)
  {
    *ppEnumFormatEtc = new ::EnumFormatEtc(1, &fmtetc);
    return S_OK;
  }
  else
    return E_NOTIMPL;
}
HRESULT __stdcall DataObject::DAdvise(FORMATETC* pFormatEtc, DWORD advf, IAdviseSink* pAdvSink,
                                      DWORD* pdwConnection)
{
  return OLE_E_ADVISENOTSUPPORTED;
}
HRESULT __stdcall DataObject::DUnadvise(DWORD dwConnection)
{
  return OLE_E_ADVISENOTSUPPORTED;
}
HRESULT __stdcall DataObject::EnumDAdvise(IEnumSTATDATA** ppEnumAdvise)
{
  return OLE_E_ADVISENOTSUPPORTED;
}

DataObject::DataObject(CLIPFORMAT format, HGLOBAL data)
{
  refCount = 1;
  fmtetc.cfFormat = format;
  fmtetc.ptd = NULL;
  fmtetc.dwAspect = DVASPECT_CONTENT;
  fmtetc.lindex = -1;
  fmtetc.tymed = TYMED_HGLOBAL;
  stgmed.tymed = TYMED_HGLOBAL;
  stgmed.hGlobal = data;
  stgmed.pUnkForRelease = NULL;
}
DataObject::~DataObject()
{
  GlobalFree(stgmed.hGlobal);
}

/////////////////////// DropSource //////////////////////////////////////

class DropSource : public IDropSource
{
public:
  HRESULT __stdcall QueryInterface(REFIID iid, void** ppvObject);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();

  HRESULT __stdcall QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState);
  HRESULT __stdcall GiveFeedback(DWORD dwEffect);

  DropSource(HCURSOR cursor);
  ~DropSource();

private:
  int refCount;
  HCURSOR hCursor;
};

HRESULT __stdcall DropSource::QueryInterface(REFIID iid, void** ppvObject)
{
  if (iid == IID_IDropSource || iid == IID_IUnknown)
  {
    AddRef();
    *ppvObject = this;
    return S_OK;
  }
  else
  {
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
}
ULONG __stdcall DropSource::AddRef()
{
  return InterlockedIncrement((LONG*) &refCount);
}
ULONG __stdcall DropSource::Release()
{
  int count = InterlockedDecrement((LONG*) &refCount);
  if (count == 0)
    delete this;
  return count;
}

HRESULT __stdcall DropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState)
{
  if (fEscapePressed)
    return DRAGDROP_S_CANCEL;
  if (!(grfKeyState & MK_LBUTTON))
    return DRAGDROP_S_DROP;
  return S_OK;
}
HRESULT __stdcall DropSource::GiveFeedback(DWORD dwEffect)
{
  if (hCursor)
  {
    SetCursor(hCursor);
    return S_OK;
  }
  else
    return DRAGDROP_S_USEDEFAULTCURSORS;
}

DropSource::DropSource(HCURSOR cursor)
  : hCursor(cursor)
  , refCount(1)
{
}
DropSource::~DropSource()
{
}

//////////////////////////////////////////////////////////////////////

ClipboardReader::ClipboardReader(CLIPFORMAT format)
{
  FORMATETC fmtetc;
  fmtetc.cfFormat = format;
  fmtetc.ptd = NULL;
  fmtetc.dwAspect = DVASPECT_CONTENT;
  fmtetc.lindex = -1;
  fmtetc.tymed = TYMED_HGLOBAL;

  if (OleGetClipboard(&pDataObj) != S_OK)
    pDataObj = NULL;
  if (pDataObj == NULL || pDataObj->GetData(&fmtetc, &stgmed) != S_OK)
    memset(&stgmed, 0, sizeof stgmed);
}
ClipboardReader::~ClipboardReader()
{
  if (stgmed.hGlobal)
    ReleaseStgMedium(&stgmed);
  if (pDataObj)
    pDataObj->Release();
}

HGLOBAL ClipboardReader::getData()
{
  return stgmed.hGlobal;
}

//////////////////////////////////////////////////////////////////////

uint32 DoDragDrop(CLIPFORMAT format, HGLOBAL data, uint32 allowedEffects, HCURSOR cursor)
{
  DropSource source(cursor);
  DataObject object(format, data);
  uint32 effect;
  uint32 result = DoDragDrop(&object, &source, allowedEffects, &effect);
  return effect;
}
uint32 DoDragDropEx(CLIPFORMAT format, HGLOBAL data, uint32 allowedEffects, HWND hWnd, HCURSOR cursor)
{
  POINT pt;
  GetCursorPos(&pt);
  bool started = false;
  uint32 startTime = GetTickCount();

  SetCapture(hWnd);
  while (!started)
  {
    if (GetCapture() != hWnd)
      break;
    MSG msg;
    if (PeekMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE) ||
      PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
    {
      if (msg.message == WM_LBUTTONUP || msg.message == WM_RBUTTONUP ||
          msg.message == WM_LBUTTONDOWN || msg.message == WM_RBUTTONDOWN)
        break;
      if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
        break;
      if (msg.pt.x < pt.x - 5 || msg.pt.y < pt.y - 5 || msg.pt.x > pt.x + 5 || msg.pt.y + pt.y + 5)
        started = true;
    }
    if (GetTickCount() - startTime > 1000)
      started = true;
  }
  ReleaseCapture();
  if (started)
    return DoDragDrop(format, data, allowedEffects, cursor);
  else
    return DROPEFFECT_NONE;
}

IDataObject* SetClipboard(CLIPFORMAT format, HGLOBAL data)
{
  DataObject* object = new DataObject(format, data);
  uint32 result = OleSetClipboard(object);
  object->Release();
  return SUCCEEDED(result) ? object : NULL;
}

char* FileListToString(Array<String> const& files)
{
  uint32 length = 1;
  for (int i = 0; i < files.length(); i++)
    length += files[i].length() + 1;
  char* ptr = new char[length];
  length = 0;
  for (int i = 0; i < files.length(); i++)
  {
    memcpy(ptr + length, files[i].c_str(), files[i].length() + 1);
    length += files[i].length() + 1;
  }
  ptr[length] = 0;
  return ptr;
}
HGLOBAL CreateFileDrop(Array<String> const& files)
{
  uint32 length = 0;
  for (int i = 0; i < files.length(); i++)
    length += files[i].length() + 1;
  uint32 size = sizeof(DROPFILES) + length + 1;
  HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, size);
  if (data)
  {
    uint8* ptr = (uint8*) GlobalLock(data);
    DROPFILES* df = (DROPFILES*) ptr;
    memset(df, 0, sizeof(DROPFILES));
    df->pFiles = sizeof(DROPFILES);
    ptr += sizeof(DROPFILES);
    for (int i = 0; i < files.length(); i++)
    {
      memcpy(ptr, files[i].c_str(), files[i].length() + 1);
      ptr += files[i].length() + 1;
    }
    *ptr = 0;
    GlobalUnlock(data);
  }
  return data;
}
HGLOBAL CreateFileDrop(String file)
{
  uint32 size = sizeof(DROPFILES) + file.length() + 2;
  HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, size);
  if (data)
  {
    uint8* ptr = (uint8*) GlobalLock(data);
    DROPFILES* df = (DROPFILES*) ptr;
    memset(df, 0, sizeof(DROPFILES));
    df->pFiles = sizeof(DROPFILES);
    ptr += sizeof(DROPFILES);
    memcpy(ptr, file.c_str(), file.length() + 1);
    ptr[file.length() + 1] = 0;
    GlobalUnlock(data);
  }
  return data;
}

String GetGlobalText(HGLOBAL data)
{
  String result;
  wchar_t* ptr = (wchar_t*) GlobalLock(data);
  if (ptr)
  {
    result = String::fromWide(ptr);
    GlobalUnlock(data);
  }
  return result;
}
WideString GetGlobalTextWide(HGLOBAL data)
{
  WideString result;
  wchar_t* ptr = (wchar_t*) GlobalLock(data);
  if (ptr)
  {
    result = WideString(ptr);
    GlobalUnlock(data);
  }
  return result;
}
HGLOBAL CreateGlobalText(String text)
{
  int size;
  wchar_t* wide = text.toWide(size);
  HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, size * 2);
  wchar_t* ptr = (wchar_t*) GlobalLock(data);
  if (ptr)
  {
    wcscpy(ptr, wide);
    GlobalUnlock(data);
  }
  delete[] wide;
  return data;
}
HGLOBAL CreateGlobalText(WideString text)
{
  HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, (text.length() + 1) * 2);
  wchar_t* ptr = (wchar_t*) GlobalLock(data);
  if (ptr)
  {
    wcscpy(ptr, text.c_str());
    GlobalUnlock(data);
  }
  return data;
}
