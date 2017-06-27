#pragma once

#include "Exception.h"
#include "Context.h"
#include "Utils.h"

class CLocker
{
  static bool s_preemption;

  std::auto_ptr<v8::Locker> m_locker;
  
  v8::Isolate* m_isolate;
public:
  CLocker();
  CLocker(CIsolatePtr isolate);
  
  bool entered(void) { return NULL != m_locker.get(); }

  void enter(void);
  void leave(void);

  bool IsLocked();
  
  static void ResetActive();

  static void Expose(void);
};

class CUnlocker
{
  std::auto_ptr<v8::Unlocker> m_unlocker;
  
  v8::Isolate* m_isolate;
public:
  CUnlocker();
  CUnlocker(CIsolatePtr isolate);
  
  bool entered(void) { return NULL != m_unlocker.get(); }

  void enter(void)
  {
    Py_BEGIN_ALLOW_THREADS

    m_unlocker.reset(new v8::Unlocker(m_isolate));

    Py_END_ALLOW_THREADS
  }
  void leave(void)
  {
    Py_BEGIN_ALLOW_THREADS

    m_unlocker.reset();

    Py_END_ALLOW_THREADS
  }
};
