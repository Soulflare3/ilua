#include "engine.h"

namespace api
{

struct SyncSection
{
  int count;
  ilua::Thread* owner;
  SyncSection()
    : owner(NULL)
  {}
  ~SyncSection()
  {
    if (owner)
      owner->release();
  }
};
struct SyncEvent
{
  int state;
};

static int sync_newsection(lua_State* L)
{
  SyncSection* t = new(L, "sync.section") SyncSection;
  t->count = 0;
  t->owner = NULL;
  return 1;
}
static int sync_newevent(lua_State* L)
{
  int state = lua_toboolean(L, 1);
  SyncEvent* t = new(L, "sync.event") SyncEvent;
  t->state = state;
  return 1;
}
static int sync_wait(lua_State* L)
{
  int count = lua_gettop(L);
  uint32 timeout = -1;
  uint32 curtime = GetTickCount();
  if (lua_getctx(L, (int*) &timeout) != LUA_YIELD && lua_isnumber(L, count))
    timeout = curtime + uint32(lua_tonumber(L, count--) * 1000.0);

  if (curtime < timeout)
  {
    lua_pushboolean(L, 0);
    return 1;
  }

  for (int i = 1; i <= count; i++)
  {
    lua_getfield(L, i, "state");
    if (!lua_iscfunction(L, lua_gettop(L)))
      luaL_argerror(L, i, "waitable object expected");
    lua_pushvalue(L, i);
    ilua::lcall(L, 1, 1);
    if (!lua_toboolean(L, -1))
    {
      lua_settop(L, count);
      return engine(L)->current_thread()->yield(sync_wait, timeout);
    }
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int section_enter(lua_State* L)
{
  SyncSection* t = ilua::checkobject<SyncSection>(L, 1, "sync.section");
  if (t->owner && t->owner->status())
  {
    t->owner->release();
    t->owner = NULL;
    t->count = 0;
  }
  ilua::Thread* cur = engine(L)->current_thread();
  if (t->count > 0 && t->owner != cur)
    return cur->yield(section_enter);
  t->owner = cur;
  t->owner->addref();
  t->count++;
  return 0;
}
static int section_leave(lua_State* L)
{
  SyncSection* t = ilua::checkobject<SyncSection>(L, 1, "sync.section");
  ilua::Thread* cur = engine(L)->current_thread();
  if (t->count > 0 && t->owner == cur)
    t->count--;
  if (t->count == 0 && t->owner)
  {
    t->owner->release();
    t->owner = NULL;
  }
  return 0;
}

static int event_state(lua_State* L)
{
  SyncEvent* t = ilua::checkobject<SyncEvent>(L, 1, "sync.event");
  lua_pushboolean(L, t->state);
  return 1;
}
static int event_set(lua_State* L)
{
  SyncEvent* t = ilua::checkobject<SyncEvent>(L, 1, "sync.event");
  t->state = 1;
  return 0;
}
static int event_reset(lua_State* L)
{
  SyncEvent* t = ilua::checkobject<SyncEvent>(L, 1, "sync.event");
  t->state = 0;
  return 0;
}

void bind_sync(lua_State* L)
{
  ilua::openlib(L, "sync");
  ilua::bindmethod(L, "newsection", sync_newsection);
  ilua::bindmethod(L, "newevent", sync_newevent);
  ilua::bindmethod(L, "wait", sync_wait);
  lua_pop(L, 1);

  ilua::newtype<SyncSection>(L, "sync.section");
  ilua::bindmethod(L, "enter", section_enter);
  ilua::bindmethod(L, "leave", section_leave);
  lua_pop(L, 2);

  ilua::newtype<SyncEvent>(L, "sync.event");
  ilua::bindmethod(L, "state", event_state);
  ilua::bindmethod(L, "set", event_set);
  ilua::bindmethod(L, "reset", event_reset);
  lua_pop(L, 2);
}

}
