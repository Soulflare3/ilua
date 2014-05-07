#include "mainwnd.h"
#include "resource.h"

#define IDC_DEBUG_THREADS       (IDC_DEBUGTAB_FIRST+0)
#define IDC_DEBUG_STACK         (IDC_DEBUGTAB_FIRST+1)
#define IDC_DEBUG_LOCALS        (IDC_DEBUGTAB_FIRST+2)
#define IDC_DEBUG_GLOBALS       (IDC_DEBUGTAB_FIRST+3)
#define IDC_DEBUG_WATCH         (IDC_DEBUGTAB_FIRST+4)

void MainWnd::onDebugStart()
{
  updateTitle();
  int sel = auxTabs[0]->getCurSel();
  if (dbg.watch == NULL)
  {
    dbg.watch = new VarList(engine, auxTabs[0], VarList::tWatch, IDC_DEBUG_WATCH);
    auxTabs[0]->addTab("Watch", dbg.watch, 0);
  }
  if (dbg.globals == NULL)
  {
    dbg.globals = new VarList(engine, auxTabs[0], VarList::tGlobals, IDC_DEBUG_GLOBALS);
    auxTabs[0]->addTab("Globals", dbg.globals, 0);
  }
  if (dbg.locals == NULL)
  {
    dbg.locals = new VarList(engine, auxTabs[0], VarList::tLocals, IDC_DEBUG_LOCALS);
    auxTabs[0]->addTab("Locals", dbg.locals, 0);
  }
  if (dbg.threads == NULL)
  {
    dbg.threads = new ListFrame(auxTabs[1], IDC_DEBUG_THREADS);
    dbg.threads->insertColumn(0, L"ID", 40, LVCFMT_RIGHT);
    dbg.threads->insertColumn(1, L"Name", 120);
    dbg.threads->insertColumn(2, L"State", 60);
    auxTabs[1]->addTab("Threads", dbg.threads, 0);
  }
  if (dbg.stack == NULL)
  {
    dbg.stack = new ListFrame(auxTabs[1], IDC_DEBUG_STACK);
    dbg.stack->insertColumn(0, L"Lvl", 40, LVCFMT_RIGHT);
    dbg.stack->insertColumn(1, L"Location", 260);
    dbg.stack->insertColumn(2, L"File", 140);
    auxTabs[1]->addTab("Stack", dbg.stack, 0);
  }
  auxTabs[0]->addTab("Output", logWindow, 0);
  if (sel >= 0)
    auxTabs[0]->setCurSel(sel + 1);

  dbg.threads->disable();
  dbg.stack->disable();
  dbg.locals->disable();
  dbg.locals->invalidate();
  dbg.globals->disable();
  dbg.globals->invalidate();
  dbg.watch->disable();
  dbg.watch->invalidate();
  dbg.curLevel = -1;

  auxTabs[0]->show();
  auxTabs[1]->show();
  //int pos = auxTabs->numTabs();
  //auxTabs->addTab("Threads", dbg.threads, dbg.iThreads);

  //auxTabs->setCurSel(0);
}
void MainWnd::onDebugEnd()
{
  updateTitle();
  if (auxTabs[0]->getTab(auxTabs[0]->getCurSel()) == logWindow)
    auxTabs[0]->setCurSel(-1);
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < auxTabs[i]->numTabs(); j++)
      if (auxTabs[i]->getTab(j) == logWindow)
        auxTabs[i]->removeTab(j);
  if (dbg.locals)
    dbg.locals->reset();
  if (dbg.globals)
    dbg.globals->reset();
  if (dbg.watch)
    dbg.watch->reset();
  logWindow->setParent(this);
  logWindow->setPoint(PT_TOPLEFT, auxTabs[0], PT_TOPLEFT, 0, 0);
  logWindow->setPoint(PT_BOTTOMRIGHT, 0, 0);
  logWindow->show();
  auxTabs[0]->hide();
  auxTabs[1]->hide();
  dbg.curLevel = -1;
  //auxTabs->removeTab(dbg.iThreads);
}
struct ThreadInfo
{
  api::Thread* thread;
  int state;
};
static int compare_threads(ThreadInfo const& a, ThreadInfo const& b)
{
  return a.thread->id() - b.thread->id();
}
void MainWnd::onDebugPause()
{
  updateTitle();
  lua_State* L = engine->lock();

  dbg.curLevel = 0;

  lua_Debug ar;

  Array<ThreadInfo> threads;
  api::Thread* curThread = NULL;
  int curState = 0;
  while (curThread = engine->getNextThread(curThread, curState))
  {
    ThreadInfo& ti = threads.push();
    ti.thread = curThread;
    ti.state = curState;
  }
  threads.sort(compare_threads);

  dbg.threads->clear();
  for (int i = 0; i < threads.length(); i++)
  {
    api::Thread* thread = threads[i].thread;
    int state = threads[i].state;

    int pos = dbg.threads->addItem(WideString(thread->id()), state == THREAD_RUNNING ? IDI_CURLINE : 0, 0);
    dbg.threads->setItemText(pos, 1, WideString(thread->name));
    if (state == THREAD_RUNNING)
      dbg.threads->setItemText(pos, 2, L"Running");
    else if (state == THREAD_YIELD)
      dbg.threads->setItemText(pos, 2, L"Yield");
    else if (state == THREAD_SLEEP)
      dbg.threads->setItemText(pos, 2, L"Sleep");
    else if (state == THREAD_SUSPEND)
      dbg.threads->setItemText(pos, 2, L"Suspended");
  }
  dbg.threads->enable();

  dbg.stack->clear();
  for (int level = 0; lua_getstack(L, level, &ar) && level < 128; level++)
  {
    if (lua_getinfo(L, "Snl", &ar))
    {
      int pos = dbg.stack->addItem(WideString(level), level == dbg.curLevel ? IDI_CURLINE : 0, level);
      WideString name;
      if (ar.name)
        name = WideString(ar.name);
      else
        name = WideString(String::getFileName(ar.source + 1));
      if (ar.currentline > 0)
        name.printf(L" (Line %d)", ar.currentline);
      dbg.stack->setItemText(pos, 1, name);
      if (ar.what[0] == 'C')
        dbg.stack->setItemText(pos, 2, L"C++");
      else
        dbg.stack->setItemText(pos, 2, WideString(String::getFileName(ar.source + 1)));
    }
  }
  dbg.stack->enable();

  dbg.locals->setState(L, dbg.curLevel);
  dbg.globals->setState(L, dbg.curLevel);
  dbg.watch->setState(L, dbg.curLevel);

  engine->unlock();
}
void MainWnd::onDebugResume()
{
  updateTitle();
  dbg.threads->disable();
  dbg.stack->disable();
  dbg.locals->disable();
  dbg.locals->invalidate();
  dbg.globals->disable();
  dbg.globals->invalidate();
  dbg.watch->disable();
  dbg.watch->invalidate();
  dbg.curLevel = -1;
}

#define VN_VARCHANGED_REAL    (VN_VARCHANGED+1)

uint32 MainWnd::onDebugMessage(uint32 message, uint32 wParam, uint32 lParam)
{
  int id = LOWORD(wParam);
  switch (id)
  {
  case IDC_DEBUG_STACK:
    if (message == WM_NOTIFY && ((NMHDR*) lParam)->code == LVN_ITEMACTIVATE)
    {
      int sel = dbg.stack->getCurSel();
      if (sel >= 0)
      {
        lua_Debug ar;
        int level = dbg.stack->getItemParam(sel);
        lua_State* L = engine->lock();
        if (lua_getstack(L, level, &ar))
        {
          dbg.curLevel = level;
          for (int i = 0; i < dbg.stack->getCount(); i++)
            dbg.stack->setItemImage(i, i == sel ? IDI_CURLINE : 0);

          lua_getinfo(L, "Sl", &ar);

          if (pathIds.has(ar.source + 1))
            PostMessage(hWnd, WM_CHANGEBREAKPOINT, pathIds.get(ar.source + 1), ar.currentline - 1);

          dbg.locals->setState(L, level);
          dbg.globals->setState(L, level);
          dbg.watch->setState(L, level);
        }
        engine->unlock();
      }
    }
    break;
  case IDC_DEBUG_LOCALS:
  case IDC_DEBUG_GLOBALS:
  case IDC_DEBUG_WATCH:
    if (HIWORD(wParam) == VN_VARCHANGED)
    {
      PostMessage(hWnd, WM_COMMAND, LOWORD(wParam) | (VN_VARCHANGED_REAL << 16), lParam);
    }
    else if (HIWORD(wParam) == VN_VARCHANGED_REAL)
    {
      //TODO: bad hax
      lua_State* L = engine->lock();
//      if (id != IDC_DEBUG_LOCALS)
        dbg.locals->setState(L, dbg.curLevel);
//      if (id != IDC_DEBUG_GLOBALS)
        dbg.globals->setState(L, dbg.curLevel);
//      if (id != IDC_DEBUG_WATCH)
        dbg.watch->setState(L, dbg.curLevel);
      engine->unlock();
    }
    break;
  }
  return 0;
}
