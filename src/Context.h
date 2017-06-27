#pragma once

#include <cassert>

#include <boost/shared_ptr.hpp>

#include "Wrapper.h"
#include "Utils.h"

class CContext;
class CIsolate;

typedef boost::shared_ptr<CContext> CContextPtr;
typedef boost::shared_ptr<CIsolate> CIsolatePtr;

class CIsolate
{
  v8::Isolate *m_isolate;
  bool m_owner;
  
private:
  static uint32_t *CalcStackLimitSize(uint32_t size);
  
public:
  CIsolate(bool owner=false);
  CIsolate(v8::Isolate *isolate);
  ~CIsolate(void);

  v8::Isolate *GetIsolate(void) { return m_isolate; }

  CJavascriptStackTracePtr GetCurrentStackTrace(int frame_limit,
    v8::StackTrace::StackTraceOptions options = v8::StackTrace::kOverview) {
    return CJavascriptStackTrace::GetCurrentStackTrace(m_isolate, frame_limit, options);
  }
  
  void Terminate();

  void Enter(void);
  void Leave(void);
  void Dispose(void);
  
  void CollectAllGarbage(bool force_compaction);
  bool SetMemoryLimit(int max_young_space_size, int max_old_space_size, int max_executable_size);
  bool SetStackLimit(uint32_t stack_limit_size);
  
  static py::object GetDefault(void);
  
  py::object GetEntered(void);
  py::object GetCurrent(void);
  py::object GetCalling(void);
  
  bool InContext(void) { return m_isolate->InContext(); }

  bool IsLocked(void) { return v8::Locker::IsLocked(m_isolate); }
};

class CContext
{
  py::object m_global;
  v8::Persistent<v8::Context> m_context;
  v8::Isolate* m_isolate;

protected:
  void Init(py::list extensions);

public:
  CContext(v8::Handle<v8::Context> context);
  CContext(const CContext& context);

  CContext(py::object global, py::list extensions) :
    m_global(global),
    m_isolate(v8::Isolate::GetCurrent())
  {
    Init(extensions);
  };

  CContext(py::object global, py::list extensions, CIsolatePtr isolate) :
    m_global(global),
    m_isolate(isolate->GetIsolate())
  {
    Init(extensions);
  };

  ~CContext()
  {
    m_context.Reset();
  }

  py::object GetGlobal(void);

  py::str GetSecurityToken(void);
  void SetSecurityToken(py::str token);

  v8::Handle<v8::Context> Handle(void) const;
  
  bool IsEntered(void);
  void Enter(void);
  void Leave(void);

  bool HasOutOfMemoryException(void);

  py::object Evaluate(const std::string& src, const std::string name = std::string(),
                      int line = -1, int col = -1, py::object precompiled = py::object());
  py::object EvaluateW(const std::wstring& src, const std::wstring name = std::wstring(),
                       int line = -1, int col = -1, py::object precompiled = py::object());

  static void Expose(void);
};
