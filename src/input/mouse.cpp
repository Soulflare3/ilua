#include <windows.h>
#include <lua/lua.hpp>

#include "ilua/ilua.h"
#include "base/thread.h"
#include "module.h"
#include "list.h"
#include "keylist.h"
#include "base/dictionary.h"
#include "base/utils.h"

static void get_coords(lua_State* L, int xi, int& x, int& y)
{
  double cx = luaL_checknumber(L, xi);
  double cy = luaL_checknumber(L, xi + 1);
  if (cx > 0 && cx < 1)
    x = int(GetSystemMetrics(SM_CXSCREEN) * cx);
  else
    x = int(cx);
  if (cy > 0 && cy < 1)
    y = int(GetSystemMetrics(SM_CYSCREEN) * cy);
  else
    y = int(cy);
}
static void get_coords(lua_State* L, int xi, double& x, double& y)
{
  x = luaL_checknumber(L, xi);
  y = luaL_checknumber(L, xi + 1);
  if (x > 0 && x < 1)
    x *= double(GetSystemMetrics(SM_CXSCREEN));
  if (y > 0 && y < 1)
    y *= double(GetSystemMetrics(SM_CYSCREEN));
}
static void sendmove(int x, int y)
{
  SetCursorPos(x, y); return;
  //int cx = GetSystemMetrics(SM_CXSCREEN);
  //int cy = GetSystemMetrics(SM_CYSCREEN);
  //if (x < 0) x = 0;
  //if (x >= cx) x = cx - 1;
  //if (y < 0) y = 0;
  //if (y >= cy) y = cy - 1;

  //INPUT inp;
  //inp.type = INPUT_MOUSE;
  //inp.mi.dx = (x * 65536 / cx);
  //inp.mi.dy = (y * 65536 / cy);
  //inp.mi.mouseData = 0;
  //inp.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
  //inp.mi.time = 0;
  //inp.mi.dwExtraInfo = 0;
  //SendInput(1, &inp, sizeof inp);
}
static void sendclicks(uint32 key)
{
  if (key)
  {
    InputList list;
    list.addkey(key);
    list.send();
  }
}

struct SlowMouse
{
  double dx;
  double dy;
  int x;
  int y;
  uint32 last;
  uint32 end;
  uint32 after;
};
static int mouse_slow_cont(lua_State* L)
{
  int ctx = -1;
  lua_getctx(L, &ctx);
  SlowMouse* sm = (SlowMouse*) lua_touserdata(L, ctx);
  uint32 cur = GetTickCount();
  if (cur > sm->end)
    cur = sm->end;
  POINT pt;
  GetCursorPos(&pt);
  sm->dx += double(sm->x - pt.x) * double(cur - sm->last) / double(sm->end - sm->last);
  sm->dy += double(sm->y - pt.y) * double(cur - sm->last) / double(sm->end - sm->last);
  int dx = int(sm->dx);
  int dy = int(sm->dy);
  if (dx || dy)
    sendmove(pt.x + dx, pt.y + dy);
  sm->dx -= dx;
  sm->dy -= dy;
  sm->last = cur;
  if (cur == sm->end)
  {
    sendclicks(sm->after);
    return 0;
  }
  else
    return ilua::engine(L)->current_thread()->yield(mouse_slow_cont, ctx);
}
static int mouse_slow(lua_State* L, int x, int y, int time, uint32 after)
{
  SlowMouse* sm = ilua::newstruct<SlowMouse>(L);
  sm->x = x;
  sm->y = y;
  sm->dx = 0;
  sm->dy = 0;
  sm->last = GetTickCount();
  sm->end = sm->last + time;
  sm->after = after;
  return ilua::engine(L)->current_thread()->yield(mouse_slow_cont, lua_gettop(L));
}

static int mouse_move(lua_State* L)
{
  int x, y;
  get_coords(L, 1, x, y);
  int time = 0;
  int index = 3;
  if (lua_isnumber(L, index))
    time = int(lua_tonumber(L, index++) * 1000.0);
  if (lua_toboolean(L, index))
  {
    POINT pt;
    GetCursorPos(&pt);
    x += pt.x;
    y += pt.y;
  }
  if (time > 0)
    return mouse_slow(L, x, y, time, 0);
  sendmove(x, y);
  return 0;
}
static int mouse_click(lua_State* L)
{
  InputModule* m = InputModule::get(L);
  uint32 key = 0;
  if (lua_type(L, 1) == LUA_TSTRING)
  {
    char const* str = lua_tostring(L, 1);
    key = InputList::parsekey(str, m->getkeymap());
  }
  else
    key = luaL_checkinteger(L, 1);
  if (lua_gettop(L) == 1)
  {
    sendclicks(key);
    return 0;
  }
  int x, y;
  get_coords(L, 2, x, y);
  int time = 0;
  int index = 4;
  if (lua_isnumber(L, index))
    time = int(lua_tonumber(L, index++) * 1000.0);
  if (lua_toboolean(L, index))
  {
    POINT pt;
    GetCursorPos(&pt);
    x += pt.x;
    y += pt.y;
  }
  if (time > 0)
    return mouse_slow(L, x, y, time, key);
  sendmove(x, y);
  sendclicks(key);
  return 0;
}
static int mouse_drag(lua_State* L)
{
  InputModule* m = InputModule::get(L);
  uint32 key = 0;
  if (lua_type(L, 1) == LUA_TSTRING)
  {
    char const* str = lua_tostring(L, 1);
    key = InputList::parsekey(str, m->getkeymap());
  }
  else
    key = luaL_checkinteger(L, 1);
  int x, y;
  get_coords(L, 2, x, y);
  int index = 4;
  if (lua_isnumber(L, index) && lua_isnumber(L, index + 1))
  {
    sendmove(x, y);
    get_coords(L, index, x, y);
    index += 2;
  }
  int time = 0;
  if (lua_isnumber(L, index))
    time = int(lua_tonumber(L, index++) * 1000.0);
  if (lua_toboolean(L, index))
  {
    POINT pt;
    GetCursorPos(&pt);
    x += pt.x;
    y += pt.y;
  }

  if ((key & (mUP | mDOWN)) == 0)
    key |= mDOWN;
  sendclicks(key);
  if (key & mUP)
    key = (key & (~mUP)) | mDOWN;
  else
    key = (key & (~mDOWN)) | mUP;
  if (time > 0)
    return mouse_slow(L, x, y, time, key);
  sendmove(x, y);
  sendclicks(key);
  return 0;
}

struct BezierCurve
{
  uint32 key;
  int cubic;
  double x[4];
  double y[4];
  uint32 t0, t1;
  uint32 prev;
};
static void bezier_xy(BezierCurve* b, double t, int& x, int& y)
{
  if (b->cubic)
  {
    x = int(
      (1 - t) * (1 - t) * (1 - t) * b->x[0] +
        3 * (1 - t) * (1 - t) * t * b->x[1] +
              3 * (1 - t) * t * t * b->x[2] +
                        t * t * t * b->x[3]
    );
    y = int(
      (1 - t) * (1 - t) * (1 - t) * b->y[0] +
        3 * (1 - t) * (1 - t) * t * b->y[1] +
              3 * (1 - t) * t * t * b->y[2] +
                        t * t * t * b->y[3]
    );
  }
  else
  {
    x = int(
      (1 - t) * (1 - t) * b->x[0] +
        2 * (1 - t) * t * b->x[1] +
                  t * t * b->x[2]
    );
    y = int(
      (1 - t) * (1 - t) * b->y[0] +
        2 * (1 - t) * t * b->y[1] +
                  t * t * b->y[2]
    );
  }
}
static void draw_segment(BezierCurve* b, uint32 t)
{
  double t0 = double(b->prev - b->t0) / double(b->t1 - b->t0);
  double t1 = double(t - b->t0) / double(b->t1 - b->t0);
  b->prev = t;

  int x0, y0;
  bezier_xy(b, t1, x0, y0);
  sendmove(x0, y0);

  //double ct = t0;
  //while (ct < t1 - 1e-3)
  //{
  //  double nt = t1;
  //  int x1, y1;
  //  while (true)
  //  {
  //    bezier_xy(b, nt, x1, y1);
  //    if ((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0) < 10)
  //      break;
  //    nt = (ct + nt) / 2;
  //  }
  //  ct = nt;
  //  x0 = x1;
  //  y0 = y1;
  //  sendmove(x0, y0);
  //}
}
static int mouse_bezier_cont(lua_State* L)
{
  int ctx = -1;
  lua_getctx(L, &ctx);
  BezierCurve* b = (BezierCurve*) lua_touserdata(L, ctx);
  uint32 cur = GetTickCount();
  if (cur > b->t1)
    cur = b->t1;
  draw_segment(b, cur);
  if (cur == b->t1)
  {
    sendclicks(b->key);
    return 0;
  }
  else
    return ilua::engine(L)->current_thread()->yield(mouse_bezier_cont, ctx);
}
static int mouse_bezier(lua_State* L)
{
  BezierCurve* b = ilua::newstruct<BezierCurve>(L);
  int bpos = lua_gettop(L);
  InputModule* m = InputModule::get(L);
  b->key = 0;
  if (lua_type(L, 1) == LUA_TSTRING)
  {
    char const* str = lua_tostring(L, 1);
    b->key = InputList::parsekey(str, m->getkeymap());
  }
  else if (!lua_isnil(L, 1))
    b->key = luaL_checkinteger(L, 1);
  get_coords(L, 2, b->x[0], b->y[0]);
  get_coords(L, 4, b->x[1], b->y[1]);
  get_coords(L, 6, b->x[2], b->y[2]);
  int index = 8;
  if (lua_isnumber(L, index) && lua_isnumber(L, index + 1))
  {
    b->cubic = 1;
    get_coords(L, index, b->x[3], b->y[3]);
    index += 2;
  }
  else
    b->cubic = 0;
  int time = 0;
  b->t0 = 0;
  b->t1 = 1;
  if (lua_isnumber(L, index))
  {
    time = int(lua_tonumber(L, index) * 1000);
    b->t0 = GetTickCount();
    b->t1 = b->t0 + time;
  }
  b->prev = b->t0;

  if ((b->key & (mUP | mDOWN)) == 0)
    b->key |= mDOWN;
  sendmove(int(b->x[0]), int(b->y[0]));
  sendclicks(b->key);
  if (b->key & mUP)
    b->key = (b->key & (~mUP)) | mDOWN;
  else
    b->key = (b->key & (~mDOWN)) | mUP;
  if (time > 0)
    return ilua::engine(L)->current_thread()->yield(mouse_bezier_cont, bpos);
  draw_segment(b, b->t1);
  sendclicks(b->key);
  return 0;
}

static int mouse_pos(lua_State* L)
{
  POINT pt;
  GetCursorPos(&pt);
  lua_pushinteger(L, pt.x);
  lua_pushinteger(L, pt.y);
  return 2;
}

void bind_mouse(lua_State* L)
{
  ilua::bindmethod(L, "move", mouse_move);
  ilua::bindmethod(L, "click", mouse_click);
  ilua::bindmethod(L, "drag", mouse_drag);
  ilua::bindmethod(L, "bezier", mouse_bezier);
  ilua::bindmethod(L, "pos", mouse_pos);
}
