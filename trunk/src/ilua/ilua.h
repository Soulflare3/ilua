#ifndef __ILUA_ILUA__
#define __ILUA_ILUA__

#include <lua/lua.hpp>
#include <typeinfo>
#include <new>

#define ILUA_TABLE_META   "ilua_meta"
#define ILUA_TABLE_BASIC  "ilua_base"
#define ILUA_TABLE_BIND   "ilua_bind"
#define ILUA_TABLE_XREF   "ilua_xref"
#define ILUA_TABLE_EXTRA  "ilua_xtra"

void luaL_printf(luaL_Buffer *B, const char *fmt, ...);

namespace ilua
{

//////////////////////////// UTILITY FUNCTIONS ///////////////////////////////

// open a new or existing global library (table)
void openlib(lua_State* L, char const* name);
// bind function to a table on top of the stack
// equivalent to lua_pushcfunction(L, func); lua_setfield(L, -2, name);
void bindmethod(lua_State* L, char const* name, lua_CFunction func);

// assign the value on top of the stack as uservalue for an object at index `pos', and pop it
void setuservalue(lua_State* L, int pos);
// push uservalue of an object at index `pos'
void getuservalue(lua_State* L, int pos);

// self-explanatory shortcuts
inline void settabss(lua_State* L, char const* n, char const* s)
{
  lua_pushstring(L, s);
  lua_setfield(L, -2, n);
}
inline void settabsi(lua_State* L, char const* n, int i)
{
  lua_pushinteger(L, i);
  lua_setfield(L, -2, n);
}
inline void settabsn(lua_State* L, char const* n)
{
  lua_pushnil(L);
  lua_setfield(L, -2, n);
}
inline int gettabsi(lua_State* L, int idx, char const* n)
{
  lua_getfield(L, idx, n);
  int result = lua_tointeger(L, -1);
  lua_pop(L, 1);
  return result;
}

// equivalent to lua_call but ensures that the current thread does not yield
void lcall(lua_State* L, int args, int ret);
// lua_callk with a pass-through continuation function (= call lua function and return whatever it returned)
int ycall(lua_State* L, int args, int ret);

// equivalent to luaL_checkoption but allows integers as well
int checkoption(lua_State* L, int narg, const char* def, const char* const lst[]);

///////////////////////////// OBJECT LIBRARY ////////////////////////////////

// internal, do not use
template<class T>
char const* basicid()
{
  return typeid(T).name();
}
template<class T>
int __typegc(lua_State* L)
{
  ((T*) lua_touserdata(L, 1))->~T();
  return 0;
}
bool __newtype(lua_State* L, char const* name, char const* parent);

// declare a new type
// use: newtype<Type>(L, "type", "base");
// or: newtype<BasicType>(L);
// pushes metatable AND __index table on the stack
template<class T>
void newtype(lua_State* L, char const* name = NULL, char const* parent = NULL)
{
  if (__newtype(L, name ? name : basicid<T>(), parent))
  {
    lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_META);
    lua_pushvalue(L, -3);
    lua_pushcfunction(L, __typegc<T>);
    lua_setfield(L, -2, "__gc");
    lua_setfield(L, -2, basicid<T>());
    lua_pop(L, 1);
  }
}

// check if `name' is equal to or derived from `base'
bool iskindof(lua_State* L, char const* name, char const* base);

} // namespace ilua
// custom allocator (has to be declared globally)
// use: Type* ptr = new(L, "type") Type(args);
// type has to be declared beforehand
void* operator new(size_t count, lua_State* L, char const* name);
void operator delete(void* ptr, lua_State* L, char const* name);
namespace ilua
{

// alternative allocator for simple structs; creates a new type if necessary
template<class T>
T* newstruct(lua_State* L, char const* name = NULL, int size = sizeof(T))
{
  T* ptr = (T*) lua_newuserdata(L, size);
  lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_META);
  lua_getfield(L, -1, name ? name : basicid<T>());
  if (!lua_istable(L, -1))
  {
    lua_pop(L, 1);
    newtype<T>(L, name);
    lua_pop(L, 1);
  }
  lua_setmetatable(L, -3);
  lua_pop(L, 1);
  new(ptr) T;
  return ptr;
}

class Engine;
class Object
{
  volatile long ref;
protected:
  friend void* ::operator new(size_t count, lua_State* L, char const* name);
  Engine* e;
public:
  Object()
    : ref(0)
  {}
  virtual ~Object() {}

  // non-zero reference count prevents lua from garbage collecting the object
  int addref();
  int release();
  int refcount() const
  {
    return ref;
  }
};

// push the original userdata associated with `obj'
void pushobject(lua_State* L, Object* obj);
// is value at index `pos' of type `name' or derived from it
bool iskindof(lua_State* L, int pos, char const* name);
template<class T>
bool iskindof(lua_State* L, int pos)
{
  pos = lua_absindex(L, pos);
  int top = lua_gettop(L);
  char const* name = basicid<T>();
  lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_META);
  lua_getfield(L, -1, name);
  if (lua_istable(L, -1))
  {
    lua_rawgeti(L, -1, 0);
    if (lua_isstring(L, -1))
      name = lua_tostring(L, -1);
  }
  bool result = iskindof(L, pos, name);
  lua_settop(L, top);
  return result;
}
// cast; if `name' is omitted, it will be determined from type
template<class T>
T* toobject(lua_State* L, int pos, char const* name = NULL)
{
  if (name ? iskindof(L, pos, name) : iskindof<T>(L, pos))
    return (T*) lua_touserdata(L, pos);
  return NULL;
}
template<class T>
T* tostruct(lua_State* L, int pos)
{
  if (!lua_isuserdata(L, pos) || !lua_getmetatable(L, pos))
    return NULL;
  lua_rawgeti(L, -1, 0);
  bool result = (lua_isstring(L, -1) && !strcmp(lua_tostring(L, -1), basicid<T>()));
  lua_pop(L, 2);
  return (result ? (T*) lua_touserdata(L, pos) : NULL);
}
template<class T>
T* checkobject(lua_State* L, int pos, char const* name = NULL)
{
  pos = lua_absindex(L, pos);
  int top = lua_gettop(L);
  if (name == NULL)
  {
    name = basicid<T>();
    lua_getfield(L, LUA_REGISTRYINDEX, ILUA_TABLE_META);
    lua_getfield(L, -1, name);
    if (lua_istable(L, -1))
    {
      lua_rawgeti(L, -1, 0);
      if (lua_isstring(L, -1))
        name = lua_tostring(L, -1);
    }
  }
  if (!iskindof(L, pos, name))
  {
    const char *msg = lua_pushfstring(L, "%s expected, got %s", name, luaL_typename(L, pos));
    luaL_argerror(L, pos, msg);
  }
  lua_settop(L, top);
  return (T*) lua_touserdata(L, pos);
}
template<class T>
T* checkstruct(lua_State* L, int pos)
{
  if (!lua_isuserdata(L, pos) || !lua_getmetatable(L, pos))
  {
    char const* msg = lua_pushfstring(L, "%s expected, got %s", basicid<T>(), luaL_typename(L, pos));
    luaL_argerror(L, pos, msg);
  }
  lua_rawgeti(L, -1, 0);
  if (!lua_isstring(L, -1) || strcmp(lua_tostring(L, -1), basicid<T>()))
  {
    char const* msg = lua_pushfstring(L, "%s expected, got %s", basicid<T>(), luaL_typename(L, pos));
    luaL_argerror(L, pos, msg);
  }
  lua_pop(L, 2);
  return (T*) lua_touserdata(L, pos);
}

// expand object to allow table-like access
// if successful, pushes values table on stack
int totable(lua_State* L, int pos);

////////////////////////////////////// ENGINE /////////////////////////////////

class Thread : public Object
{
public:
  // get thread state
  virtual lua_State* state() = 0;
  // suspend thread (does not stop current execution, use lua_yield afterwards)
  virtual void suspend() = 0;
  // use instead of lua_yield to briefly pause execution
  virtual int yield(lua_CFunction cont = NULL, int ctx = 0) = 0;
  // same as yield but waits for specified number of milliseconds
  virtual int sleep(int time, lua_CFunction cont = NULL, int ctx = 0) = 0;
  // resume suspended thread, passing specified number of args or an object to continuation function
  virtual void resume(Object* rc = NULL) = 0;
  virtual void resume(int args) = 0;

  // stop thread execution (does not stop current execution, use lua_yield afterwards)
  virtual void terminate() = 0;

  // prevent thread from yielding
  virtual void lock() = 0;
  virtual void unlock() = 0;

  // thread id
  virtual int id() const = 0;

  // 1 if finished
  virtual int status() const = 0;

  // create/get an opaque block of data associated with the thread
  virtual void* newdata(int size) = 0;
  virtual void* data() = 0;
};

class Engine
{
public:
  // load a file and execute it as a new thread
  virtual bool load(char const* file) = 0;
  // load a DLL module
  virtual bool load_module(char const* name) = 0;

  // pause the engine to allow modifying the state
  // returns the state of the currently executing thread, or global state
  virtual lua_State* lock() = 0;
  virtual void unlock() = 0;

  // execution counters
  // `id' is the counter identifier
  // creating a thread decreases counter by 1 and increases it when it finishes execution
  virtual int get_counter(int id) = 0;
  virtual void set_counter(int id, int value) = 0;
  // create a thread from a function and specified number of arguments; optional execution counter (see above)
  virtual Thread* create_thread(int narg, int counter = 0) = 0;
  // currently executing thread
  virtual Thread* current_thread() = 0;

  // initially the execution stops when all threads have exited
  // calling keepalive causes the engine to run until exit is called explicitly
  virtual void keepalive() = 0;

  // stops execution
  virtual void exit(int code) = 0;
};

// get engine associated with the state
Engine* engine(lua_State* L);

} // namespace ilua

#endif // __ILUA_ILUA__
