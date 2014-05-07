#ifndef __UI_VARLIST__
#define __UI_VARLIST__

#include "core/frameui/framewnd.h"
#include "base/array.h"
#include "base/wstring.h"
#include "base/dictionary.h"
#include "base/hashmap.h"
#include "core/api/engine.h"

#define VN_VARCHANGED           100

class VarList : public WindowFrame
{
  union KeyType
  {
    double number;
    uint64 lua;
    struct
    {
      int index;
      uint32 type;
    };
  };
  struct VarData
  {
    KeyType key;
    WideString name;
    uint32 valtype;
    WideString value;
    WideString valtypename;

    enum {fChanged = 1, fExpanded = 2, fLvalue = 4};
    uint32 flags;
    int ypos; // relative to parent
    int yheight;

    VarData* parent;
    int pindex;
    Array<VarData*> children;
  };
  VarData root;
  VarData* selected;
  bool _selfound;
  int localLevel;
  int listType;

  int scrollPos;
  int scrollAccum;
  HICON ticons[2];
  HPEN gridpen;
  HFONT fonts[3];
  HCURSOR cursors[3];

  api::Engine* engine;

  static KeyType luaget(lua_State* L, int idx);
  static void luapush(lua_State* L, KeyType val);

  VarData* getNodeByPos(int ypos, int* depth);
  int getNodePos(VarData* node, int* depth);
  void invalidateNode(VarData* node);
  void ensureVisible(VarData* node);
  void sortnode(VarData* node);
  void pushnode(lua_State* L, VarData* node);
  bool setnode(lua_State* L, VarData* node);
  void resetnode(VarData* node);
  void delnode(VarData* node);
  void removenode(VarData* node);
  void erasenode(VarData* node);

  void onchanged();

  // key = to the right of name
  enum {htNone, htExpand, htName, htKey, htType, htValue};
  VarData* hitTest(POINT pt, int* code);
  RECT getNodeRect(VarData* node, int code);

  VarData* getChild(VarData* node, KeyType key);
  static int sortNodes(VarData* const& a, VarData* const& b);
  void nodeSizeChanged(VarData* node);
  void updateNodeValue(lua_State* L, VarData* node);
  VarData* addChild(lua_State* L, VarData* node, KeyType key, HashMap<KeyType, VarData*>* prev);
  void updateNode(lua_State* L, VarData* node);
  void doScroll(int pos);

  WideString getEditInit(VarData* node, bool key);
  void editLabel(VarData* node, bool key, uint32 delay = 0);
  void startEdit();
  void cancelEdit();
  void delEdit();
  bool finishEdit(VarData* node, bool key, wchar_t const* str);
  void select(VarData* node);
  bool expandable(VarData* node);
  void toggleExpand(VarData* node);
  bool canEdit(VarData* node, bool key);

  class LabelEditor;
  friend class LabelEditor;
  LabelEditor* editor;

  struct RenderContext
  {
    HDC hDC;
    int ymin;
    int ymax;

    int colx[3];
    Array<bool> lines;
  };
  void renderNode(RenderContext& rx, int xpos, int ypos, VarData* node);

  uint32 onMessage(uint32 message, uint32 wParam, uint32 lParam);
public:
  enum {tGlobals, tLocals, tWatch};
  VarList(api::Engine* e, Frame* parent, int type, int id = 0);
  ~VarList();

  void setState(lua_State* L, int local = -1);
  void reset();
};

#endif // __UI_VARLIST__
