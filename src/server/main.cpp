#include <windows.h>
#include "ilua/ilua.h"
#include "ilua/stream.h"
#include "base/thread.h"
#include "mongoose.h"

class Server : public ilua::Object
{
  static int mgHandler(mg_connection* con, mg_event ev);
  int threadProc();
  lua_CFunction fCompile;
  lua_CFunction fMatch;
  mg_server* server;
  bool shutdown;
  thread::Counter pause;
  CRITICAL_SECTION lock;
  HANDLE hThread;
public:
  Server(lua_CFunction compile, lua_CFunction match);
  ~Server();
  void destroy();
  void setOption(char const* opt, char const* value);
  char const* getOption(char const* opt)
  {
    return mg_get_option(server, opt);
  }
  void addPath(lua_State* L);

  void waitStart()
  {
    pause.increment();
  }
  bool waitTry()
  {
    return TryEnterCriticalSection(&lock);
  }
  void waitFinish()
  {
    pause.decrement();
    LeaveCriticalSection(&lock);
  }
};
class Connection : public ilua::Stream
{
  ilua::Engine* e;
  mg_connection* con;
  ilua::Thread* thread;
  enum {bufSize = 256};
  char buffer[bufSize];
  int bufCount;
  bool written;
public:
  Connection(lua_State* L, mg_connection* c);
  ~Connection()
  {
    if (thread)
      thread->release();
    con->connection_param = NULL;
  }
  void setThread(ilua::Thread* t)
  {
    if (thread)
      thread->release();
    thread = t;
    if (thread)
      thread->addref();
  }
  int onEvent(mg_event ev);

  mg_connection* connection()
  {
    return con;
  }

  int write(void const* buf, int count);
  void flush();
};

Connection::Connection(lua_State* L, mg_connection* c)
  : con(c)
  , thread(NULL)
  , bufCount(0)
  , e(ilua::engine(L))
  , written(false)
{
  addref();
  con->connection_param = this;
  if (ilua::totable(L, -1))
  {
    ilua::settabss(L, "method", con->request_method);
    ilua::settabss(L, "uri", con->uri);
    ilua::settabss(L, "httpversion", con->http_version);
    ilua::settabss(L, "query", con->query_string);
    ilua::settabss(L, "remoteip", con->remote_ip);
    ilua::settabss(L, "localip", con->local_ip);
    ilua::settabsi(L, "remoteport", con->remote_port);
    ilua::settabsi(L, "localport", con->local_port);
    lua_newtable(L);
    for (int i = 0; i < con->num_headers; i++)
      ilua::settabss(L, con->http_headers[i].name, con->http_headers[i].value);
    lua_setfield(L, -2, "headers");
    ilua::pushbuffer(L, con->content, con->content_len);
    lua_setfield(L, -2, "content");
    lua_pop(L, 1);
  }
}
int Connection::onEvent(mg_event ev)
{
  switch (ev)
  {
  case MG_AUTH:
    return MG_TRUE;
  case MG_POLL:
    if (thread)
    {
      if (thread->status() == 0)
        return MG_FALSE;
      if (written)
      {
        flush();
        mg_send_data(con, NULL, 0);
      }
    }
    return MG_TRUE;
  case MG_CLOSE:
    release();
  }
  return MG_FALSE;
}
int Connection::write(void const* buf, int count)
{
  written = true;
  char const* cbuf = (char const*) buf;
  int add = 0;
  if (bufCount)
  {
    add = bufSize - bufCount;
    if (add > count) add = count;
    memcpy(buffer + bufCount, cbuf, add);
    bufCount += add;
    cbuf += add;
    count -= add;
    if (bufCount >= bufSize)
      flush();
  }
  if (bufCount + count >= bufSize)
    mg_send_data(con, cbuf, count);
  else
  {
    memcpy(buffer + bufCount, cbuf, count);
    bufCount += count;
  }
  return add + count;
}
void Connection::flush()
{
  if (bufCount)
  {
    mg_send_data(con, buffer, bufCount);
    bufCount = 0;
  }
}

int Server::mgHandler(mg_connection* con, mg_event ev)
{
  Server* srv = (Server*) con->server_param;
  if (con->connection_param)
    return ((Connection*) con->connection_param)->onEvent(ev);
  switch (ev)
  {
  case MG_AUTH:
    return MG_TRUE;
  case MG_REQUEST:
    {
      lua_State* L = srv->e->lock();
      ilua::pushobject(L, srv);
      lua_getuservalue(L, -1);
      if (lua_istable(L, -1))
      {
        int n = lua_rawlen(L, -1);
        for (int i = 1; i <= n; i++)
        {
          lua_rawgeti(L, -1, i);
          if (lua_istable(L, -1))
          {
            int top = lua_gettop(L);
            lua_pushcfunction(L, srv->fMatch);
            lua_rawgeti(L, top, 1);
            lua_pushstring(L, con->uri);
            ilua::lcall(L, 2, LUA_MULTRET);
            if (!lua_isnoneornil(L, top + 1))
            {
              Connection* cobj = new(L, "server.connection") Connection(L, con);
              lua_insert(L, top + 1);
              lua_rawgeti(L, top, 2);
              lua_insert(L, top + 1);
              int args = lua_gettop(L) - top - 1;
              cobj->setThread(srv->e->create_thread(args));
              lua_pop(L, 3);
              srv->e->unlock();
              return MG_MORE;
            }
            lua_settop(L, top);
          }
          lua_pop(L, 1);
        }
      }
      lua_pop(L, 2);
      srv->e->unlock();
    }
    return MG_FALSE;
  default:
    return MG_FALSE;
  }
}
int Server::threadProc()
{
  while (!shutdown)
  {
    pause.wait(INFINITE);
    EnterCriticalSection(&lock);
    mg_poll_server(server, 100);
    LeaveCriticalSection(&lock);
  }
  return 0;
}

Server::Server(lua_CFunction compile, lua_CFunction match)
  : fCompile(compile)
  , fMatch(match)
  , server(NULL)
  , shutdown(false)
  , hThread(NULL)
{
  InitializeCriticalSection(&lock);
  server = mg_create_server(this, mgHandler);
}
Server::~Server()
{
  destroy();
  DeleteCriticalSection(&lock);
}
void Server::destroy()
{
  shutdown = true;
  if (hThread)
  {
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    hThread = NULL;
    release();
  }
  mg_destroy_server(&server);
}
void Server::addPath(lua_State* L)
{
  lua_getuservalue(L, 1);
  if (lua_isnil(L, -1))
  {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setuservalue(L, 1);
  }
  lua_newtable(L);
  if (lua_isstring(L, 2))
  {
    lua_pushcfunction(L, fCompile);
    lua_pushvalue(L, 2);
    lua_pushinteger(L, 0x80000001); // PCRE_CASELESS | RE_LIST
    ilua::lcall(L, 2, 1);
    lua_replace(L, 2);
  }
  ilua::checkobject<int>(L, 2, "re.prog");
  lua_pushvalue(L, 2);
  lua_rawseti(L, -2, 1);
  luaL_argcheck(L, lua_isfunction(L, 3), 3, "function expected");
  lua_pushvalue(L, 3);
  lua_rawseti(L, -2, 2);
  lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
  lua_pop(L, 1);
}
void Server::setOption(char const* opt, char const* value)
{
  mg_set_option(server, opt, value);
  if (!strcmp(opt, "listening_port") && hThread == NULL)
  {
    addref();
    e->keepalive();
    hThread = thread::create(this, &Server::threadProc);
  }
}

static int server_create(lua_State* L)
{
  char const* port = luaL_optstring(L, 1, NULL);
  lua_getglobal(L, "re");
  if (!lua_istable(L, -1)) luaL_error(L, "global regex library has been modified");
  lua_getfield(L, -1, "compile");
  lua_CFunction fCompile = lua_tocfunction(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, -1, "match");
  lua_CFunction fMatch = lua_tocfunction(L, -1);
  lua_pop(L, 2);
  if (!fCompile || !fMatch) luaL_error(L, "global regex library has been modified");
  Server* srv = new(L, "server") Server(fCompile, fMatch);
  if (port)
    srv->setOption("listening_port", port);
  return 1;
}

static int server_shutdown(lua_State* L)
{
  Server* srv = ilua::checkobject<Server>(L, 1, "server");
  srv->destroy();
  return 0;
}
static int server_option_cont(lua_State* L)
{
  Server* srv = ilua::checkobject<Server>(L, 1, "server");
  if (!srv->waitTry())
    return lua_yieldk(L, 0, 0, server_option_cont);
  char const* name = luaL_checkstring(L, 2);
  char const* value = luaL_optstring(L, 3, NULL);
  int top = lua_gettop(L);
  char const* old = srv->getOption(name);
  if (old)
    lua_pushstring(L, old);
  else
    lua_pushnil(L);
  if (top > 2)
    srv->setOption(name, value);
  srv->waitFinish();
  return 1;
}
static int server_option(lua_State* L)
{
  Server* srv = ilua::checkobject<Server>(L, 1, "server");
  srv->waitStart();
  return server_option_cont(L);
}
static int server_route_cont(lua_State* L)
{
  Server* srv = ilua::checkobject<Server>(L, 1, "server");
  if (!srv->waitTry())
    return lua_yieldk(L, 0, 0, server_route_cont);
  srv->addPath(L);
  srv->waitFinish();
  return 0;
}
static int server_route(lua_State* L)
{
  Server* srv = ilua::checkobject<Server>(L, 1, "server");
  srv->waitStart();
  return server_route_cont(L);
}

static int con_sendstatus(lua_State* L)
{
  Connection* con = ilua::checkobject<Connection>(L, 1, "server.connection");
  mg_send_status(con->connection(), luaL_checkinteger(L, 2));
  return 0;
}
static int con_sendheader(lua_State* L)
{
  Connection* con = ilua::checkobject<Connection>(L, 1, "server.connection");
  mg_send_header(con->connection(), ilua::checkbuffer(L, 2, NULL), ilua::checkbuffer(L, 3, NULL));
  return 0;
}

static int server_doredirect(lua_State* L)
{
  Connection* con = ilua::checkobject<Connection>(L, 1, "server.connection");
  mg_send_status(con->connection(), lua_tointeger(L, lua_upvalueindex(2)));
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_insert(L, 2);
  lua_getglobal(L, "re");
  lua_getfield(L, -1, "expand");
  lua_insert(L, 2);
  lua_pop(L, 1);
  ilua::lcall(L, lua_gettop(L) - 2, 1);
  if (lua_isstring(L, -1))
    mg_send_header(con->connection(), "Location", lua_tostring(L, -1));
  return 0;
}
static int server_redirect(lua_State* L)
{
  luaL_checkstring(L, 1);
  if (lua_isnoneornil(L, 2))
  {
    lua_settop(L, 1);
    lua_pushinteger(L, 302);
  }
  luaL_checkinteger(L, 2);
  lua_settop(L, 2);
  lua_pushcclosure(L, server_doredirect, 2);
  return 1;
}

extern "C" __declspec(dllexport) void StartModule(lua_State* L)
{
  ilua::newtype<Server>(L, "server", "object");
  ilua::bindmethod(L, "shutdown", server_shutdown);
  ilua::bindmethod(L, "option", server_option);
  ilua::bindmethod(L, "route", server_route);
  lua_pop(L, 2);

  ilua::newtype<Connection>(L, "server.connection", "stream");
  ilua::stream_noseek(L);
  ilua::stream_noread(L);
  ilua::bindmethod(L, "sendstatus", con_sendstatus);
  ilua::bindmethod(L, "sendheader", con_sendheader);
  lua_pop(L, 2);

  ilua::openlib(L, "server");
  ilua::bindmethod(L, "create", server_create);
  ilua::bindmethod(L, "redirect", server_redirect);
  lua_pop(L, 1);
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
  return TRUE;
}
