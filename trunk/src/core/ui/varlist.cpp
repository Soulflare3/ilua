extern "C"
{

  #include <stdarg.h>
  #include <string.h>

  #define lapi_c
  #define LUA_CORE

  #include "lua/lua.h"

  #include "lua/lapi.h"
  #include "lua/ldebug.h"
  #include "lua/ldo.h"
  #include "lua/lfunc.h"
  #include "lua/lgc.h"
  #include "lua/lmem.h"
  #include "lua/lobject.h"
  #include "lua/lstate.h"
  #include "lua/lstring.h"
  #include "lua/ltable.h"
  #include "lua/ltm.h"
  #include "lua/lundump.h"
  #include "lua/lvm.h"

}

#include "varlist.h"
#include "core/app.h"
#include "core/frameui/fontsys.h"
#include "resource.h"
#include "luaeval.h"
#include <windowsx.h>

#define KEY_LOCAL       0x7FF7A5FF
#define KEY_PSEUDO      0x7FF7A5FE
#define KEY_WATCH       0x7FF7A5FD
#define KPI_VARARG      0
#define KPI_ADDKEY      1
#define KPI_METATABLE   2
#define KPI_NEWMETA     3
#define KPI_ADDWATCH    4

#define HEADER_HEIGHT       0
#define ITEM_HEIGHT         17
#define ITEM_WIDTH          16


struct LuaType
{
  bool expandable;
  int sort;

  uint32 color;
} _luat[] = {
  {false, 0, 0x0000FF}, // none
  {false, 0, 0xFF0000}, // nil
  {false, 2, 0xFF0000}, // boolean
  {false, 4, 0x808080}, // light userdata
  {false, 1, 0x000000}, // number
  {false, 3, 0x000000}, // string
  { true, 4, 0x808080}, // table
  {false, 4, 0x808080}, // function
  { true, 4, 0x808080}, // userdata
  {false, 4, 0x808080}, // thread
};
static LuaType* luat = &_luat[1];

static inline int luatype(int t)
{
  return (t & NNMASK) == NNMARK ? (t & 0x80 ? t & 0xFF : t & 0x0F) : LUA_TNUMBER;
}
static inline bool expandable(int t)
{
  return t == LUA_TTABLE || t == LUA_TUSERDATA;
}
static bool isIdStr(char const* str, int len = -1)
{
  if (len < 0) len = strlen(str);
  if (len == 0) return false;
  if (s_isdigit(str[0])) return false;
  for (int p = 0; p < len; p++)
    if (!s_isalnum(str[p]) && str[p] != '_')
      return false;
  return true;
}
static String formatkey(lua_State* L, int idx)
{
  switch (lua_type(L, idx))
  {
  case LUA_TNONE:
  case LUA_TNIL:
    return "[nil]";
  case LUA_TBOOLEAN:
    return lua_toboolean(L, idx) ? "[true]" : "[false]";
  case LUA_TNUMBER:
    return String::format("[%.14g]", lua_tonumber(L, idx));
  case LUA_TSTRING:
    {
      size_t len;
      char const* str = lua_tolstring(L, idx, &len);
      if (isIdStr(str, len))
        return str;
      else
        return String::format("[\"%s\"]", luae::formatstr(str, len));
    }
    break;
  default:
    return String::format("[%s: %p]", lua_typename(L, lua_type(L, -1)), lua_topointer(L, -1));
  }
}
static String formatval(lua_State* L, int idx)
{
  switch (lua_type(L, idx))
  {
  case LUA_TNIL:
    return "nil";
  case LUA_TBOOLEAN:
    return lua_toboolean(L, idx) ? "true" : "false";
  case LUA_TNUMBER:
    return String::format("%.14g", lua_tonumber(L, idx));
  case LUA_TSTRING:
    {
      size_t len;
      char const* str = lua_tolstring(L, idx, &len);
      return String::format("\"%s\"", luae::formatstr(str, len));
    }
    break;
  default:
    return "";
  }
}

VarList::KeyType VarList::luaget(lua_State* L, int idx)
{
  return *(KeyType*) (L->top + idx);
}
void VarList::luapush(lua_State* L, KeyType val)
{
  *(KeyType*) (L->top++) = val;
}
int VarList::sortNodes(VarData* const& a, VarData* const& b)
{
  int ta = luatype(a->key.type);
  int tb = luatype(b->key.type);
  if (ta > LUA_TTHREAD || tb > LUA_TTHREAD)
  {
    if (ta != tb)
      return ta - tb;
    return a->key.index - b->key.index;
  }
  if (luat[ta].sort != luat[tb].sort)
    return luat[ta].sort - luat[tb].sort;
  if (ta == LUA_TNUMBER)
  {
    if (a->key.number < b->key.number)
      return -1;
    if (a->key.number > b->key.number)
      return 1;
    return 0;
  }
  else if (ta == LUA_TSTRING)
  {
    char const* sa = (char const*) (((TString*) a->key.index) + 1);
    char const* sb = (char const*) (((TString*) b->key.index) + 1);
    return strcmp(sa, sb);
  }
  else
    return a->key.index - b->key.index;
}

void VarList::nodeSizeChanged(VarData* node)
{
  while (node->parent)
  {
    if (!(node->parent->flags & VarData::fExpanded))
      return;
    int index = node->pindex;
    node->parent->yheight = node->ypos;
    node = node->parent;
    for (; index < node->children.length(); index++)
    {
      node->children[index]->ypos = node->yheight;
      node->yheight += (node->children[index]->flags & VarData::fExpanded
        ? node->children[index]->yheight : 1);
    }
    if (node->parent)
      node->yheight++;
  }

  RECT rc;
  GetClientRect(hWnd, &rc);
  SCROLLINFO si;
  memset(&si, 0, sizeof si);
  si.cbSize = sizeof si;
  si.fMask = SIF_RANGE | SIF_PAGE;
  si.nMin = 0;
  si.nMax = root.yheight - 1;
  si.nPage = rc.bottom / ITEM_HEIGHT;
  SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
}
void VarList::updateNodeValue(lua_State* L, VarData* node)
{
  if (node == selected)
    _selfound = true;

  int oldtype = node->valtype;
  WideString oldvalue = node->value;

  node->valtype = lua_type(L, -1);
  node->value = luae::towstring(L, -1);
  if (node->valtype == LUA_TUSERDATA && lua_getmetatable(L, -1))
  {
    lua_rawgeti(L, -1, 0);
    if (char const* name = lua_tostring(L, -1))
      node->valtypename = WideString(String::format("(%s)", name));
    lua_pop(L, 2);
  }
  node->flags = (node->flags & (~VarData::fChanged)) |
    (((oldtype != node->valtype || oldvalue != node->value) && oldtype != LUA_TNONE)
     ? VarData::fChanged : 0);
}
VarList::VarData* VarList::addChild(lua_State* L, VarData* node, KeyType key, HashMap<KeyType, VarData*>* prev)
{
  VarData* sub = NULL;
  if (prev && prev->has(key))
  {
    sub = prev->get(key);
    prev->del(key);
  }
  else
  {
    sub = new VarData;
    sub->valtype = LUA_TNONE;
    sub->flags = 0;
    sub->key = key;
    sub->parent = node;
  }
  node->children.push(sub);

  updateNodeValue(L, sub);
  updateNode(L, sub);

  lua_pop(L, 1);

  return sub;
}
void VarList::sortnode(VarData* node)
{
  node->children.sort(sortNodes);
  node->yheight = 0;
  for (int i = 0; i < node->children.length(); i++)
  {
    node->children[i]->ypos = node->yheight;
    node->children[i]->pindex = i;
    node->yheight += (node->children[i]->flags & VarData::fExpanded
      ? node->children[i]->yheight : 1);
  }
  if (node->parent)
    node->yheight++;
}
void VarList::updateNode(lua_State* L, VarData* node)
{
  node->yheight = 1;
  if (luat[lua_type(L, -1)].expandable && (node->flags & VarData::fExpanded))
  {
    HashMap<KeyType, VarData*> prev;
    for (int i = 0; i < node->children.length(); i++)
      prev.set(node->children[i]->key, node->children[i]);
    node->children.clear();
    if (lua_type(L, -1) == LUA_TTABLE)
    {
      lua_pushnil(L);
      while (lua_next(L, -2))
      {
        KeyType key = luaget(L, -2);
        VarData* sub = addChild(L, node, key, &prev);
        sub->name = WideString(formatkey(L, -1));
      }

      KeyType key;
      key.type = KEY_PSEUDO;
      key.index = KPI_ADDKEY;
      lua_pushnil(L);
      addChild(L, node, key, &prev)->name = L"(add key)";
    }
    if (lua_getmetatable(L, -1))
    {
      KeyType key;
      key.type = KEY_PSEUDO;
      key.index = KPI_METATABLE;
      addChild(L, node, key, &prev)->name = L"(metatable)";
    }
    else if (node->parent && (lua_type(L, -1) == LUA_TTABLE || lua_type(L, -1) == LUA_TUSERDATA))
    {
      KeyType key;
      key.type = KEY_PSEUDO;
      key.index = KPI_NEWMETA;
      lua_pushnil(L);
      addChild(L, node, key, &prev)->name = L"(create metatable)";
    }
    sortnode(node);

    for (uint32 cur = prev.enumStart(); cur; cur = prev.enumNext(cur))
      delnode(prev.enumGetValue(cur));
  }
  else if (node->key.type == KEY_PSEUDO && node->key.index == KPI_VARARG &&
          (node->flags & VarData::fExpanded))
  {
    HashMap<KeyType, VarData*> prev;
    for (int i = 0; i < node->children.length(); i++)
      prev.set(node->children[i]->key, node->children[i]);
    node->children.clear();

    lua_Debug ar;
    if (lua_getstack(L, localLevel, &ar))
    {
      for (int i = 1; lua_getlocal(L, &ar, -i); i++)
      {
        KeyType key;
        key.type = KEY_LOCAL;
        key.index = -i;
        addChild(L, node, key, &prev)->name = WideString::format(L"[%d]", i);
      }
    }
    sortnode(node);

    for (uint32 cur = prev.enumStart(); cur; cur = prev.enumNext(cur))
      delnode(prev.enumGetValue(cur));
  }
  else
  {
    for (int i = 0; i < node->children.length(); i++)
      delnode(node->children[i]);
    node->children.clear();
  }
}

void VarList::setState(lua_State* L, int local)
{
  enable();
  cancelEdit();
  localLevel = local;

  _selfound = false;
  if (listType == tGlobals)
  {
    lua_pushglobaltable(L);
    updateNode(L, &root);
    lua_pop(L, 1);
  }
  else if (listType == tLocals)
  {
    root.yheight = 0;
    lua_Debug ar;
    if (lua_getstack(L, local, &ar))
    {
      HashMap<KeyType, VarData*> prev;
      for (int i = 0; i < root.children.length(); i++)
        prev.set(root.children[i]->key, root.children[i]);

      root.children.clear();
      for (int i = 0; char const* name = lua_getlocal(L, &ar, i + 1); i++)
      {
        if (name[0] == '(')
        {
          lua_pop(L, 1);
          continue;
        }
        KeyType key;
        key.type = KEY_LOCAL;
        key.index = i + 1;
        addChild(L, &root, key, &prev)->name = WideString(name);
      }

      if (lua_getlocal(L, &ar, -1))
      {
        lua_pop(L, 1);
        lua_pushnil(L);
        KeyType key;
        key.type = KEY_PSEUDO;
        key.index = KPI_VARARG;
        addChild(L, &root, key, &prev)->name = L"(...)";
      }
      for (int i = 0; i < root.children.length(); i++)
      {
        root.children[i]->ypos = root.yheight;
        root.children[i]->pindex = i;
        root.yheight += (root.children[i]->flags & VarData::fExpanded
          ? root.children[i]->yheight : 1);
      }

      for (uint32 cur = prev.enumStart(); cur; cur = prev.enumNext(cur))
        delnode(prev.enumGetValue(cur));
    }
    else
    {
      for (int i = 0; i < root.children.length(); i++)
        delnode(root.children[i]);
      root.children.clear();
    }
  }
  else if (listType == tWatch)
  {
    for (int i = 0; i < root.children.length(); i++)
    {
      VarData* node = root.children[i];
      if (node->key.type != KEY_WATCH)
        continue;
      int code = luae::getvalue(L, localLevel, String(node->name));
      if (code == luae::eFail)
        lua_pushnil(L);
      node->flags = (node->flags & (~VarData::fLvalue)) |
        (luae::islvalue(code) ? VarData::fLvalue : 0);
      updateNodeValue(L, node);
      updateNode(L, node);
      lua_pop(L, 1);
    }
    KeyType key;
    key.type = KEY_PSEUDO;
    key.index = KPI_ADDWATCH;
    if (!getChild(&root, key))
    {
      lua_pushnil(L);
      addChild(L, &root, key, NULL)->name = L"(add watch)";
    }
    sortnode(&root);
  }
  if (!_selfound)
    selected = NULL;
  if (selected)
    ensureVisible(selected);
  nodeSizeChanged(&root);
  invalidate();
}
void VarList::delnode(VarData* node)
{
  for (int i = 0; i < node->children.length(); i++)
    delnode(node->children[i]);
  if (selected == node)
    selected = node->parent;
  delete node;
}
void VarList::removenode(VarData* node)
{
  node->parent->children.remove(node->pindex);
  sortnode(node->parent);
  nodeSizeChanged(node->parent);
  delnode(node);
}
void VarList::resetnode(VarData* node)
{
  node->valtype = LUA_TNONE;
  node->value = L"";
  for (int i = 0; i < node->children.length(); i++)
    resetnode(node->children[i]);
}
void VarList::reset()
{
  cancelEdit();
  resetnode(&root);
}

VarList::VarList(api::Engine* e, Frame* parent, int type, int id)
  : WindowFrame(parent)
  , scrollPos(0)
  , scrollAccum(0)
  , engine(e)
  , selected(NULL)
  , listType(type)
  , editor(NULL)
{
  root.key.lua = 0;
  root.valtype = LUA_TTABLE;
  root.flags = VarData::fExpanded;
  root.ypos = 0;
  root.yheight = 0;
  root.parent = NULL;
  root.pindex = 0;

  ticons[0] = (HICON) LoadImage(getInstance(), MAKEINTRESOURCE(IDI_TPLUS), IMAGE_ICON, 16, 16, 0);
  ticons[1] = (HICON) LoadImage(getInstance(), MAKEINTRESOURCE(IDI_TMINUS), IMAGE_ICON, 16, 16, 0);
  fonts[0] = FontSys::getSysFont();
  fonts[1] = FontSys::getFont(12, L"Courier New", 0);
  fonts[2] = FontSys::changeFlags(FONT_UNDERLINE);
  cursors[0] = LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW));
  cursors[1] = LoadCursor(NULL, MAKEINTRESOURCE(IDC_HAND));
  cursors[2] = LoadCursor(NULL, MAKEINTRESOURCE(IDC_IBEAM));
  gridpen = CreatePen(PS_SOLID, 1, 0x808080);

  if (WNDCLASSEX* wcx = createclass(L"iLuaVarList"))
  {
    wcx->style |= CS_DBLCLKS;
    wcx->hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    RegisterClassEx(wcx);
  }
  create(L"", WS_CHILD | WS_TABSTOP | WS_VSCROLL, WS_EX_CLIENTEDGE);
  setId(id);
}
VarList::~VarList()
{
  DeleteObject(cursors[0]);
  DeleteObject(cursors[1]);
  DeleteObject(cursors[2]);
  DeleteObject(gridpen);
  delEdit();
}

VarList::VarData* VarList::getNodeByPos(int ypos, int* depth)
{
  if (depth) *depth = -1;
  VarData* node = &root;
  while (ypos != 0 || node->parent == NULL)
  {
    if (!(node->flags & VarData::fExpanded))
      return NULL;
    if (node->parent != NULL)
      ypos--;
    int left = 0;
    int right = node->children.length();
    while (left < right - 1)
    {
      int mid = (left + right) / 2;
      if (node->children[mid]->ypos > ypos)
        right = mid;
      else
        left = mid;
    }
    node = node->children[left];
    ypos -= node->ypos;
    if (depth) (*depth)++;
  }
  return node;
}
int VarList::getNodePos(VarData* node, int* depth)
{
  int ypos = 0;
  if (depth) *depth = -2;
  while (node)
  {
    ypos += node->ypos;
    node = node->parent;
    if (node && node->parent)
      ypos++;
    if (depth) (*depth)++;
  }
  return ypos;
}
void VarList::pushnode(lua_State* L, VarData* node)
{
  if (node->key.type == KEY_LOCAL)
  {
    lua_Debug ar;
    if (!lua_getstack(L, localLevel, &ar) || !lua_getlocal(L, &ar, node->key.index))
      lua_pushnil(L);
  }
  else if (node->key.type == KEY_WATCH)
  {
    if (luae::getvalue(L, localLevel, String(node->name)) == luae::eFail)
      lua_pushnil(L);
  }
  else if (node->key.type == KEY_PSEUDO && node->key.index != KPI_METATABLE)
    lua_pushnil(L);
  else if (node->parent)
  {
    pushnode(L, node->parent);
    if (node->key.type == KEY_PSEUDO && node->key.index == KPI_METATABLE)
    {
      if (!lua_getmetatable(L, -1))
        lua_pushnil(L);
    }
    else
    {
      luapush(L, node->key);
      lua_rawget(L, -2);
    }
    lua_remove(L, -2);
  }
  else
    lua_pushglobaltable(L);
}
bool VarList::setnode(lua_State* L, VarData* node)
{
  if (node->key.type == KEY_LOCAL)
  {
    lua_Debug ar;
    if (!lua_getstack(L, localLevel, &ar) || !lua_setlocal(L, &ar, node->key.index))
    {
      lua_pop(L, 1);
      return false;
    }
  }
  else if (node->key.type == KEY_WATCH)
    return luae::setvalue(L, localLevel, String(node->name)) != luae::eFail;
  else if (node->key.type == KEY_PSEUDO && node->key.index != KPI_METATABLE)
    lua_pop(L, 1);
  else if (node->parent)
  {
    pushnode(L, node->parent);
    lua_insert(L, -2);
    if (node->key.type == KEY_PSEUDO && node->key.index == KPI_METATABLE)
    {
      if (!lua_istable(L, -1))
      {
        lua_pop(L, 2);
        return false;
      }
      lua_setmetatable(L, -2);
    }
    else
    {
      luapush(L, node->key);
      lua_insert(L, -2);
      lua_rawset(L, -3);
    }
    lua_pop(L, 1);
  }
  else
  {
    lua_pop(L, 1);
    return false;
  }
  return true;
}
VarList::VarData* VarList::getChild(VarData* node, KeyType key)
{
  for (int i = 0; i < node->children.length(); i++)
    if (node->children[i]->key.lua == key.lua)
      return node->children[i];
  return NULL;
}
bool VarList::finishEdit(VarData* node, bool key, wchar_t const* str)
{
  String cstr(str);
  if (key)
  {
    if (node->key.type == KEY_WATCH)
    {
      if (str == NULL || *str == 0)
      {
        erasenode(node);
        return true;
      }
      lua_State* L = engine->lock();
      int code = luae::getvalue(L, localLevel, cstr);
      if (code != luae::eFail)
      {
        node->flags = (luae::islvalue(code) ? VarData::fLvalue : 0);
        node->name = str;
        node->valtype = LUA_TNONE;
        updateNodeValue(L, node);
        updateNode(L, node);
        lua_pop(L, 1);
        nodeSizeChanged(node);
        selected = node;
        invalidate();
      }
      engine->unlock();
      return code != luae::eFail;
    }
    else if (node->key.type != KEY_LOCAL && node->key.type != KEY_PSEUDO)
    {
      if (str == NULL || *str == 0)
      {
        erasenode(node);
        return true;
      }
      lua_State* L = engine->lock();
      bool success = true;
      if (isIdStr(cstr))
        lua_pushstring(L, cstr);
      else
        success = (luae::getvalue(L, localLevel, cstr) != luae::eFail);
      if (success)
      {
        if (lua_isnil(L, -1))
        {
          erasenode(node);
          engine->unlock();
          return true;
        }
        pushnode(L, node->parent);
        lua_insert(L, -2);
        lua_pushvalue(L, -1);
        lua_rawget(L, -3);
        success = lua_isnil(L, -1);
        lua_pop(L, 1);
        if (success)
        {
          KeyType newkey = luaget(L, -1);
          node->name = WideString(formatkey(L, -1));
          luapush(L, node->key);
          lua_rawget(L, -3);
          lua_rawset(L, -3);
          luapush(L, node->key);
          lua_pushnil(L);
          lua_rawset(L, -3);
          node->key = newkey;
          lua_pop(L, 1);
          sortnode(node->parent);
          ensureVisible(node);
          selected = node;
          invalidate();
          onchanged();
        }
        else
          lua_pop(L, 2);
      }
      engine->unlock();
      return success;
    }
    return false;
  }
  lua_State* L = engine->lock();
  bool success = (luae::getvalue(L, localLevel, cstr) != luae::eFail);
  if (success)
  {
    //if (lua_isnil(L, -1) && node->key.type != KEY_WATCH && node->key.type != KEY_LOCAL)
    //{
    //  lua_pop(L, 1);
    //  erasenode(node);
    //  engine->unlock();
    //  return true;
    //}
    lua_pushvalue(L, -1);
    success = setnode(L, node);
    if (success)
    {
      node->flags &= (~(VarData::fChanged | VarData::fExpanded));
      node->valtype = LUA_TNONE;
      updateNodeValue(L, node);
      updateNode(L, node);
      nodeSizeChanged(node);
      selected = node;
      invalidate();
      onchanged();
    }
    lua_pop(L, 1);
  }
  engine->unlock();
  return success;
}
WideString VarList::getEditInit(VarData* node, bool key)
{
  if (key)
  {
    if (node->key.type == KEY_WATCH)
      return node->name;
    if (node->key.type != KEY_LOCAL && node->key.type != KEY_PSEUDO)
    {
      lua_State* L = engine->lock();
      luapush(L, node->key);
      WideString result(formatval(L, -1));
      lua_pop(L, 1);
      engine->unlock();
      return result;
    }
    return L"";
  }
  if (node->valtype == LUA_TNONE)
    return L"";
  lua_State* L = engine->lock();
  pushnode(L, node);
  WideString result(formatval(L, -1));
  lua_pop(L, 1);
  engine->unlock();
  return result;
}
void VarList::erasenode(VarData* node)
{
  if (node->key.type == KEY_WATCH)
  {
    KeyType key = node->key;
    for (key.index++; VarData* next = getChild(node->parent, key); key.index++)
      next->key.index--;
    removenode(node);
  }
  else if (node->key.type == KEY_PSEUDO && node->key.index == KPI_METATABLE)
  {
    VarData* parent = node->parent;
    lua_State* L = engine->lock();
    pushnode(L, parent);
    if (lua_type(L, -1) == LUA_TTABLE || lua_type(L, -1) == LUA_TUSERDATA)
    {
      lua_pushnil(L);
      lua_setmetatable(L, -2);
      KeyType key;
      key.type = KEY_PSEUDO;
      key.index = KPI_NEWMETA;
      lua_pushnil(L);
      VarData* added = addChild(L, parent, key, NULL);
      added->name = L"(create metatable)";
      removenode(node);
      node = added;
      onchanged();
    }
    lua_pop(L, 1);
    engine->unlock();
  }
  else if (node->key.type != KEY_LOCAL && node->key.type != KEY_PSEUDO)
  {
    lua_State* L = engine->lock();
    lua_pushnil(L);
    if (setnode(L, node))
      removenode(node);
    engine->unlock();
    onchanged();
  }
  else
    return;
  invalidate();
}

/////////////////////////////////////////////////////////////////////
// rendering

void VarList::invalidateNode(VarData* node)
{
  RECT rc;
  GetClientRect(hWnd, &rc);
  int oheight = rc.bottom;
  int ypos = getNodePos(node, NULL);
  rc.top = HEADER_HEIGHT + (ypos - scrollPos) * ITEM_HEIGHT + 1;
  rc.bottom = rc.top + ITEM_HEIGHT - 1;
  if (rc.top < 0) rc.top = 0;
  if (rc.bottom > oheight) rc.bottom = oheight;
  if (rc.bottom > rc.top)
    InvalidateRect(hWnd, &rc, TRUE);
}
VarList::VarData* VarList::hitTest(POINT pt, int* code)
{
  if (code) *code = htNone;
  int ypos = ((pt.y - HEADER_HEIGHT) / ITEM_HEIGHT) + scrollPos;
  pt.y = (pt.y - HEADER_HEIGHT) % ITEM_HEIGHT;
  int depth;
  VarData* node = getNodeByPos(ypos, &depth);
  if (node == NULL || code == NULL)
    return node;

  RECT rc;
  GetClientRect(hWnd, &rc);
  int colx[3];
  colx[0] = (rc.right - 60) / 3;
  colx[1] = colx[0] + 60;
  colx[2] = rc.right;

  SIZE nameSize = FontSys::getTextSize(fonts[0], node->name);

  if ((luat[node->valtype].expandable || (node->key.type == KEY_PSEUDO && node->key.index == KPI_VARARG)) && 
      pt.x >= ITEM_WIDTH * depth + 3 && pt.x < ITEM_WIDTH * depth + 12 && pt.y >= 4 && pt.y < 13)
    *code = htExpand;
  else if (pt.x < (depth + 1) * ITEM_WIDTH)
    *code = htNone;
  else if (pt.x < (depth + 1) * ITEM_WIDTH + nameSize.cx)
    *code = htName;
  else if (pt.x < colx[0])
    *code = htKey;
  else if (pt.x < colx[1])
    *code = htType;
  else if (pt.x < colx[2])
    *code = htValue;
  else
    *code = htNone;
  return node;
}
RECT VarList::getNodeRect(VarData* node, int code)
{
  int depth;
  int ypos = getNodePos(node, &depth);
  RECT rc;
  GetClientRect(hWnd, &rc);
  int colx[3];
  colx[0] = (rc.right - 60) / 3;
  colx[1] = colx[0] + 60;
  colx[2] = rc.right;
  rc.top = HEADER_HEIGHT + (ypos - scrollPos) * ITEM_HEIGHT + 1;
  rc.bottom = rc.top + ITEM_HEIGHT - 1;
  switch (code)
  {
  case htExpand:
    rc.top = rc.top + 4;
    rc.left = ITEM_WIDTH * depth + 3;
    rc.bottom = rc.top + 9;
    rc.right = rc.left + 9;
    break;
  case htName:
    rc.left = (depth + 1) * ITEM_WIDTH;
    rc.right = rc.left + FontSys::getTextSize(fonts[0], node->name).cx;
    break;
  case htKey:
    rc.left = (depth + 1) * ITEM_WIDTH;
    rc.right = colx[0];
    break;
  case htType:
    rc.left = colx[0] + 6;
    rc.right = colx[1];
    break;
  case htValue:
    rc.left = colx[1] + 6;
    rc.right = colx[2];
    break;
  }
  return rc;
}
void VarList::renderNode(RenderContext& rx, int xpos, int ypos, VarData* node)
{
  ypos += node->ypos;
  if (ypos >= rx.ymax)
    return;
  if (node != &root)
  {
    if (ypos >= rx.ymin)
    {
      RECT rc;
      rc.top = HEADER_HEIGHT + ypos * ITEM_HEIGHT + 1;
      rc.bottom = HEADER_HEIGHT + (ypos + 1) * ITEM_HEIGHT;

      bool disabled = (!IsWindowEnabled(hWnd));
      bool sel = ((node == selected && GetFocus() == hWnd) || disabled);
      if (sel)
      {
        if (disabled)
        {
          SetBkColor(rx.hDC, 0xFFFFFF);
          SetTextColor(rx.hDC, 0x808080);
        }
        else
        {
          SetBkColor(rx.hDC, 0x6A240A);
          SetTextColor(rx.hDC, 0xFFFFFF);
          rc.left = 0;
          rc.right = rx.colx[2];
          ExtTextOut(rx.hDC, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
        }
      }
      else
        SetBkColor(rx.hDC, 0xFFFFFF);

      for (int x = 0; x < xpos; x++)
      {
        if (x == xpos - 1 && node->pindex == node->parent->children.length() - 1)
        {
          MoveToEx(rx.hDC, x * ITEM_WIDTH + 7, rc.top, NULL);
          LineTo(rx.hDC, x * ITEM_WIDTH + 7, rc.top + 9);
          rx.lines[x] = false;
        }
        else if (rx.lines[x])
        {
          MoveToEx(rx.hDC, x * ITEM_WIDTH + 7, rc.top, NULL);
          LineTo(rx.hDC, x * ITEM_WIDTH + 7, rc.bottom);
        }
      }
      if (xpos)
      {
        MoveToEx(rx.hDC, (xpos - 1) * ITEM_WIDTH + 7, rc.top + 8, NULL);
        LineTo(rx.hDC, xpos * ITEM_WIDTH + 7, rc.top + 8);
      }

      if ((node->flags & VarData::fExpanded) || luat[node->valtype].expandable ||
        (node->key.type == KEY_PSEUDO && node->key.index == KPI_VARARG))
      {
        if (node->flags & VarData::fExpanded)
        {
          MoveToEx(rx.hDC, xpos * ITEM_WIDTH + 7, rc.top + 8, NULL);
          LineTo(rx.hDC, xpos * ITEM_WIDTH + 7, rc.bottom);
        }
        DrawIconEx(rx.hDC, xpos * ITEM_WIDTH, rc.top, ticons[node->flags & VarData::fExpanded ? 1 : 0],
          16, 16, 0, NULL, DI_IMAGE | DI_MASK);
      }

      rc.top++;

      if (!sel)
        SetTextColor(rx.hDC, node->key.type == KEY_PSEUDO ? 0xFF0000 : 0x000000);
      if (node->key.type == KEY_PSEUDO && node->key.index != KPI_METATABLE && node->key.index != KPI_VARARG)
        SelectObject(rx.hDC, fonts[2]);
      else if (node->key.type == KEY_WATCH)
        SelectObject(rx.hDC, fonts[1]);
      else
        SelectObject(rx.hDC, fonts[0]);
      rc.left = (xpos + 1) * ITEM_WIDTH;
      rc.right = rx.colx[0];
      DrawText(rx.hDC, node->name, node->name.length(), &rc, DT_LEFT | DT_VCENTER | DT_NOPREFIX);

      if (node->key.type != KEY_PSEUDO || node->key.index == KPI_METATABLE)
      {
        if (!sel)
          SetTextColor(rx.hDC, node->valtypename.length() ? 0xFF0000 : 0x000000);
        SelectObject(rx.hDC, fonts[0]);
        rc.left = rx.colx[0] + 6;
        rc.right = rx.colx[1];
        DrawText(rx.hDC,
          node->valtypename.length() ? node->valtypename.c_str() :
          WideString(lua_typename(NULL, node->valtype)),
          -1, &rc, DT_LEFT | DT_VCENTER | DT_NOPREFIX);

        if (!sel)
          SetTextColor(rx.hDC, node->flags & VarData::fChanged ? 0x0000FF : luat[node->valtype].color);
        SelectObject(rx.hDC, fonts[1]);
        rc.left = rx.colx[1] + 6;
        rc.right = rx.colx[2];
        DrawText(rx.hDC, node->value, node->value.length(), &rc, DT_LEFT | DT_VCENTER | DT_NOPREFIX);
      }
    }
    xpos++;
    ypos++;
  }
  while (xpos > rx.lines.length())
    rx.lines.push(true);
  if (xpos)
    rx.lines[xpos - 1] = true;
  if (node->flags & VarData::fExpanded)
    for (int i = 0; i < node->children.length(); i++)
      renderNode(rx, xpos, ypos, node->children[i]);
}
void VarList::select(VarData* node)
{
  if (selected != node)
  {
    if (selected)
      invalidateNode(selected);
    selected = node;
    if (selected)
      invalidateNode(selected);
  }
}
bool VarList::expandable(VarData* node)
{
  return luat[node->valtype].expandable || (node->key.type == KEY_PSEUDO && node->key.index == KPI_VARARG);
}
bool VarList::canEdit(VarData* node, bool key)
{
  if (key)
    return node->key.type != KEY_LOCAL && node->key.type != KEY_PSEUDO;
  else
    return (node->key.type != KEY_PSEUDO || node->key.index == KPI_METATABLE) &&
           (node->key.type != KEY_WATCH || (node->flags & VarData::fLvalue));
}
void VarList::toggleExpand(VarData* node)
{
  if (expandable(node))
  {
    node->flags ^= VarData::fExpanded;
    if ((node->flags & VarData::fExpanded) && node->children.length() == 0)
    {
      lua_State* L = engine->lock();
      pushnode(L, node);
      updateNode(L, node);
      lua_pop(L, 1);
      engine->unlock();
    }
    nodeSizeChanged(node);
    invalidate();
  }
}
uint32 VarList::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  switch (message)
  {
  case WM_PAINT:
    {
      RECT rc;
      GetClientRect(hWnd, &rc);

      PAINTSTRUCT ps;
      RenderContext rx;
      rx.hDC = BeginPaint(hWnd, &ps);

      SelectObject(rx.hDC, gridpen);

      rx.ymin = ((ps.rcPaint.top - HEADER_HEIGHT) / ITEM_HEIGHT);
      rx.ymax = ((ps.rcPaint.bottom - HEADER_HEIGHT - 1) / ITEM_HEIGHT) + 1;
      if (rx.ymax > root.yheight - scrollPos)
        rx.ymax = root.yheight - scrollPos;

      rx.colx[0] = (rc.right - 60) / 3;
      rx.colx[1] = rx.colx[0] + 60;
      rx.colx[2] = rc.right;

      renderNode(rx, 0, -scrollPos, &root);

      for (int i = rx.ymin; i <= rx.ymax; i++)
      {
        if (i == 0) continue;
        MoveToEx(rx.hDC, ps.rcPaint.left, i * ITEM_HEIGHT + HEADER_HEIGHT, NULL);
        LineTo(rx.hDC, ps.rcPaint.right, i * ITEM_HEIGHT + HEADER_HEIGHT);
      }
      for (int j = 0; j < sizeof rx.colx / sizeof rx.colx[0]; j++)
      {
        MoveToEx(rx.hDC, rx.colx[j], ps.rcPaint.top, NULL);
        LineTo(rx.hDC, rx.colx[j], rx.ymax * ITEM_HEIGHT + HEADER_HEIGHT);
      }

      EndPaint(hWnd, &ps);
    }
    break;
  case WM_SIZE:
    {
      SCROLLINFO si;
      memset(&si, 0, sizeof si);
      si.cbSize = sizeof si;
      si.fMask = SIF_PAGE;
      si.nPage = HIWORD(lParam) / ITEM_HEIGHT;
      SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
    }
    break;
  case WM_SETCURSOR:
    if (LOWORD(lParam) == HTCLIENT)
    {
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hWnd, &pt);
      int code;
      VarData* node = hitTest(pt, &code);
      if (node)
      {
        if (node->key.type == KEY_PSEUDO && node->key.index != KPI_VARARG && node->key.index != KPI_METATABLE && code == htName)
        {
          SetCursor(cursors[1]);
          return TRUE;
        }
        if (node == selected && node->key.type != KEY_LOCAL && node->key.type != KEY_PSEUDO &&
            (code == htName || code == htKey))
        {
          SetCursor(cursors[2]);
          return TRUE;
        }
        if (node == selected && (node->key.type != KEY_PSEUDO || node->key.index == KPI_METATABLE) && code == htValue)
        {
          SetCursor(cursors[2]);
          return TRUE;
        }
      }
    }
    return M_UNHANDLED;
  case WM_LBUTTONDBLCLK:
  case WM_LBUTTONDOWN:
    {
      SetFocus(hWnd);
      cancelEdit();

      POINT pt;
      pt.x = GET_X_LPARAM(lParam);
      pt.y = GET_Y_LPARAM(lParam);
      int code;
      VarData* node = hitTest(pt, &code);
      if (node)
      {
        if (message == WM_LBUTTONDBLCLK && expandable(node))
          code = htExpand;
        if (code == htExpand)
          toggleExpand(node);
        else if (node->key.type == KEY_PSEUDO && code == htName)
        {
          if (node->key.index == KPI_ADDKEY)
          {
            node = node->parent;
            lua_State* L = engine->lock();
            pushnode(L, node);
            if (lua_istable(L, -1))
            {
              int n = lua_rawlen(L, -1) + 1;
              KeyType key;
              key.number = n;
              while (getChild(node, key))
              {
                n++;
                key.number = n;
              }
              lua_pushnil(L);
              VarData* added = addChild(L, node, key, NULL);
              added->name = WideString::format(L"[%d]", n);
              sortnode(node);
              nodeSizeChanged(node);
              ensureVisible(added);
              invalidate();
              node = added;
              lua_pop(L, 1);
              editLabel(node, true);
            }
            else
              lua_pop(L, 1);
            engine->unlock();
          }
          else if (node->key.index == KPI_NEWMETA)
          {
            VarData* parent = node->parent;
            lua_State* L = engine->lock();
            pushnode(L, parent);
            if (lua_type(L, -1) == LUA_TTABLE || lua_type(L, -1) == LUA_TUSERDATA)
            {
              lua_newtable(L);
              lua_pushvalue(L, -1);
              lua_setmetatable(L, -3);
              KeyType key;
              key.type = KEY_PSEUDO;
              key.index = KPI_METATABLE;
              VarData* added = addChild(L, parent, key, NULL);
              added->name = L"(metatable)";
              removenode(node);
              invalidate();
              node = added;
              onchanged();
            }
            lua_pop(L, 1);
            engine->unlock();
          }
          else if (node->key.index == KPI_ADDWATCH)
          {
            KeyType key;
            key.type = KEY_WATCH;
            key.index = 0;
            for (int i = 0; i < root.children.length(); i++)
              if (root.children[i]->key.type == KEY_WATCH && root.children[i]->key.index >= key.index)
                key.index = root.children[i]->key.index + 1;
            lua_State* L = engine->lock();
            lua_pushnil(L);
            node = addChild(L, &root, key, NULL);
            node->name = L"";
            sortnode(&root);
            nodeSizeChanged(&root);
            ensureVisible(node);
            invalidate();
            editLabel(node, true);
            engine->unlock();
          }
        }
        else if (node == selected)
        {
          if ((code == htName || code == htKey) && canEdit(node, true))
            editLabel(node, true, 750);
          else if (code == htValue && canEdit(node, false))
            editLabel(node, false, 750);
        }
      }
      select(node);
    }
    break;
  case WM_KEYDOWN:
    switch (wParam)
    {
    case VK_DELETE:
      if (selected)
        erasenode(selected);
      break;
    case VK_UP:
      if (selected && selected->parent)
      {
        int line = getNodePos(selected, NULL);
        VarData* next = getNodeByPos(line - 1, NULL);
        if (next)
          select(next);
      }
      else if (root.children.length())
        select(root.children[0]);
      break;
    case VK_DOWN:
      if (selected)
      {
        int line = getNodePos(selected, NULL);
        VarData* next = getNodeByPos(line + 1, NULL);
        if (next)
          select(next);
      }
      else if (root.children.length())
        select(root.children[0]);
      break;
    case VK_LEFT:
      if (selected)
      {
        if (expandable(selected) && (selected->flags & VarData::fExpanded))
          toggleExpand(selected);
        else if (selected->parent && selected->parent->parent)
          select(selected->parent);
      }
      else if (root.children.length())
        select(root.children[0]);
      break;
    case VK_RIGHT:
      if (selected && expandable(selected))
      {
        if (!(selected->flags & VarData::fExpanded))
          toggleExpand(selected);
        else if (selected->children.length() > 0)
          select(selected->children[0]);
      }
      else if (root.children.length())
        select(root.children[0]);
      break;
    case VK_SPACE:
      if (selected)
        toggleExpand(selected);
      break;
    case VK_RETURN:
      if (selected && canEdit(selected, false))
        editLabel(selected, false);
      break;
    }
    break;
  case WM_VSCROLL:
    {
      SCROLLINFO si;
      memset(&si, 0, sizeof si);
      si.cbSize = sizeof si;
      si.fMask = SIF_ALL;
      GetScrollInfo(hWnd, SB_VERT, &si);
      switch (LOWORD(wParam))
      {
      case SB_TOP:
        si.nPos = si.nMin;
        break;
      case SB_BOTTOM:
        si.nPos = si.nMax;
        break;
      case SB_LINEUP:
        si.nPos--;
        break;
      case SB_LINEDOWN:
        si.nPos++;
        break;
      case SB_PAGEUP:
        si.nPos -= si.nPage;
        break;
      case SB_PAGEDOWN:
        si.nPos += si.nPage;
        break;
      case SB_THUMBTRACK:
        si.nPos = si.nTrackPos;
        break;
      }
      doScroll(si.nPos);
    }
    return 0;
  case WM_MOUSEWHEEL:
    {
      int step;
      SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &step, 0);
      if (step < 0)
        step = 3;
      scrollAccum += GET_WHEEL_DELTA_WPARAM(wParam) * step;
      doScroll(scrollPos - scrollAccum / WHEEL_DELTA);
      scrollAccum %= WHEEL_DELTA;
    }
    return 0;
  case WM_TIMER:
    startEdit();
    KillTimer(hWnd, wParam);
    return 0;
  default:
    return M_UNHANDLED;
  }
  return 0;
}
void VarList::doScroll(int pos)
{
  cancelEdit();
  SCROLLINFO si;
  memset(&si, 0, sizeof si);
  si.cbSize = sizeof si;
  si.fMask = SIF_RANGE | SIF_PAGE;
  GetScrollInfo(hWnd, SB_VERT, &si);
  if (pos < si.nMin) pos = si.nMin;
  if (pos > si.nMax - si.nPage + 1) pos = si.nMax - si.nPage + 1;
  si.fMask = SIF_POS;
  if (pos != scrollPos)
  {
    si.nPos = pos;
    SetScrollInfo(hWnd, SB_VERT, &si, TRUE);

    int deltaY = scrollPos - pos;
    scrollPos = pos;
    RECT rc;
    GetClientRect(hWnd, &rc);
    rc.top = HEADER_HEIGHT + 1;
    ScrollWindowEx(hWnd, 0, deltaY * ITEM_HEIGHT, &rc, &rc, NULL, NULL, SW_ERASE | SW_INVALIDATE);
  }
}
void VarList::ensureVisible(VarData* node)
{
  int ypos = getNodePos(node, NULL);

  SCROLLINFO si;
  memset(&si, 0, sizeof si);
  si.cbSize = sizeof si;
  si.fMask = SIF_PAGE;
  GetScrollInfo(hWnd, SB_VERT, &si);
  
  int yto = scrollPos;
  if (ypos < yto)
    yto = ypos;
  else if (ypos >= yto + si.nPage)
    yto = ypos - si.nPage + 1;
  doScroll(yto);
}
