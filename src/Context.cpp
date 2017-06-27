#include "Context.h"

#include "Wrapper.h"
#include "Engine.h"

void CContext::Expose(void)
{
  py::class_<CIsolate, boost::noncopyable>("JSIsolate", "JSIsolate is an isolated instance of the V8 engine.", py::no_init)
    .def(py::init<bool>((py::arg("owner") = false)))

    .add_property("locked", &CIsolate::IsLocked)

    .def("GetCurrentStackTrace", &CIsolate::GetCurrentStackTrace)
    .def("terminate", &CIsolate::Terminate)

    .def("enter", &CIsolate::Enter,
         "Sets this isolate as the entered one for the current thread. "
         "Saves the previously entered one (if any), so that it can be "
         "restored when exiting.  Re-entering an isolate is allowed.")

    .def("leave", &CIsolate::Leave,
         "Exits this isolate by restoring the previously entered one in the current thread. "
         "The isolate may still stay the same, if it was entered more than once.")

    .add_static_property("default", &CIsolate::GetDefault,
                         "Returns the default isolate.")
                         

    .def("setMemoryLimit", &CIsolate::SetMemoryLimit, (py::arg("max_young_space_size") = 0,
                                                      py::arg("max_old_space_size") = 0,
                                                      py::arg("max_executable_size") = 0),
         "Specifies the limits of the runtime's memory use."
         "You must set the heap size before initializing the VM"
         "the size cannot be adjusted after the VM is initialized.")
         

    .def("collect", &CIsolate::CollectAllGarbage, (py::arg("force")=true),
         "Performs a full garbage collection. Force compaction if the parameter is true (use for testing purposes only).")

    .def("setStackLimit", &CIsolate::SetStackLimit, (py::arg("stack_limit_size") = 0),
         "Uses the address of a local variable to determine the stack top now."
         "Given a size, returns an address that is that far from the current top of stack.")
    
    .add_property("entered", &CIsolate::GetEntered,
                         "The last entered context.")
    .add_property("current", &CIsolate::GetCurrent,
                         "The context that is on the top of the stack.")
    .add_property("calling", &CIsolate::GetCalling,
                         "The context of the calling JavaScript code.")
    .add_property("inContext", &CIsolate::InContext,
                         "Returns true if V8 has a current context.")
    ;

  py::class_<CContext, boost::noncopyable>("JSContext", "JSContext is an execution context.", py::no_init)
    .def(py::init<const CContext&>("create a new context base on a exists context"))

    .def(py::init<py::object, py::list, CIsolatePtr>((
                                        py::arg("global") = py::object(),
                                        py::arg("extensions") = py::list(),
                                        py::arg("isolate")),
                                        "Create a new context base on global object for the given isolate"))

    .def(py::init<py::object, py::list>((py::arg("global") = py::object(),
                                        py::arg("extensions") = py::list()),
                                        "Create a new context base on global object for the current isolate"))

    .add_property("securityToken", &CContext::GetSecurityToken, &CContext::SetSecurityToken)

    .add_property("locals", &CContext::GetGlobal, "Local variables within context")

    .add_property("hasOutOfMemoryException", &CContext::HasOutOfMemoryException)

    .def("eval", &CContext::Evaluate, (py::arg("source"),
                                       py::arg("name") = std::string(),
                                       py::arg("line") = -1,
                                       py::arg("col") = -1,
                                       py::arg("precompiled") = py::object()))
    .def("eval", &CContext::EvaluateW, (py::arg("source"),
                                        py::arg("name") = std::wstring(),
                                        py::arg("line") = -1,
                                        py::arg("col") = -1,
                                        py::arg("precompiled") = py::object()))

    .def("enter", &CContext::Enter, "Enter this context. "
         "After entering a context, all code compiled and "
         "run is compiled and run in this context.")
    .def("leave", &CContext::Leave, "Exit this context. "
         "Exiting the current context restores the context "
         "that was in place when entering the current context.")

    .def("__nonzero__", &CContext::IsEntered, "the context has been entered.")
    ;

  py::objects::class_value_wrapper<boost::shared_ptr<CIsolate>,
    py::objects::make_ptr_instance<CIsolate,
    py::objects::pointer_holder<boost::shared_ptr<CIsolate>,CIsolate> > >();

  py::objects::class_value_wrapper<boost::shared_ptr<CContext>,
    py::objects::make_ptr_instance<CContext,
    py::objects::pointer_holder<boost::shared_ptr<CContext>,CContext> > >();
}

// Uses the address of a local variable to determine the stack top now.
// Given a size, returns an address that is that far from the current
// top of stack.
uint32_t *CIsolate::CalcStackLimitSize(uint32_t size)
{
  uint32_t* answer = &size - (size / sizeof(size));

  // If the size is very large and the stack is very near the bottom of
  // memory then the calculation above may wrap around and give an address
  // that is above the (downwards-growing) stack.  In that case we return
  // a very low address.
  if (answer > &size) return reinterpret_cast<uint32_t*>(sizeof(size));

  return answer;
}

void CIsolate::CollectAllGarbage(bool force_compaction)
{
  v8::HandleScope handle_scope(m_isolate);

  if (force_compaction)
  {
    v8::internal::FLAG_expose_gc = true;
    m_isolate->RequestGarbageCollectionForTesting(v8::Isolate::kFullGarbageCollection);
  }
  else
  {
    while(!v8::V8::IdleNotification()) {};
  }
}

bool CIsolate::SetStackLimit(uint32_t stack_limit_size)
{
  v8::ResourceConstraints limit;

  limit.set_stack_limit(CalcStackLimitSize(stack_limit_size));

  return v8::SetResourceConstraints(m_isolate, &limit);
}

bool CIsolate::SetMemoryLimit(int max_young_space_size, int max_old_space_size, int max_executable_size)
{
  v8::ResourceConstraints limit;

  if (max_young_space_size) limit.set_max_young_space_size(max_young_space_size);
  if (max_old_space_size) limit.set_max_old_space_size(max_old_space_size);
  if (max_executable_size) limit.set_max_executable_size(max_executable_size);

  return v8::SetResourceConstraints(m_isolate, &limit);
}

CIsolate::CIsolate(bool owner) : m_owner(owner)
{
    m_isolate = v8::Isolate::New();
}

CIsolate::CIsolate(v8::Isolate *isolate)
    : m_isolate(isolate),
    m_owner(false)
{}

CIsolate::~CIsolate(void)
{
    if (m_owner)
        m_isolate->Dispose();
}

void CIsolate::Enter(void)
{
    m_isolate->Enter();
}

void CIsolate::Leave(void)
{
    m_isolate->Exit();
}

void CIsolate::Dispose(void)
{
    m_isolate->Dispose();
}

void CIsolate::Terminate()
{
    v8::V8::TerminateExecution(m_isolate);
}

py::object CIsolate::GetDefault(void)
{
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  v8::HandleScope handle_scope(isolate);
  return py::object(py::handle<>(boost::python::converter::shared_ptr_to_python<CIsolate>(CIsolatePtr(new CIsolate(isolate)))));
}

py::object CIsolate::GetEntered(void)
{
  v8::HandleScope handle_scope(m_isolate);
  v8::Handle<v8::Context> entered = m_isolate->GetEnteredContext();

  return (!m_isolate->InContext() || entered.IsEmpty()) ? py::object() :
    py::object(py::handle<>(boost::python::converter::shared_ptr_to_python<CContext>(CContextPtr(new CContext(entered)))));
}

py::object CIsolate::GetCurrent(void)
{
  v8::HandleScope handle_scope(m_isolate);

  v8::Handle<v8::Context> current = m_isolate->GetCurrentContext();

  return (current.IsEmpty()) ? py::object() :
    py::object(py::handle<>(boost::python::converter::shared_ptr_to_python<CContext>(CContextPtr(new CContext(current)))));
}

py::object CIsolate::GetCalling(void)
{
  v8::HandleScope handle_scope(m_isolate);

  v8::Handle<v8::Context> calling = m_isolate->GetCallingContext();

  return (!m_isolate->InContext() || calling.IsEmpty()) ? py::object() :
    py::object(py::handle<>(boost::python::converter::shared_ptr_to_python<CContext>(CContextPtr(new CContext(calling)))));
}

CContext::CContext(v8::Handle<v8::Context> context)
{
  m_isolate = context->GetIsolate();
  
  v8::HandleScope handle_scope(m_isolate);
  m_context.Reset(context->GetIsolate(), context);
}


v8::Handle<v8::Context> CContext::Handle(void) const
{
    return v8::Local<v8::Context>::New(m_isolate, m_context);
}
  
bool CContext::IsEntered(void)
{
    return !m_context.IsEmpty();
}

void CContext::Enter(void)
{
    v8::HandleScope handle_scope(m_isolate);
    Handle()->Enter();
}

void CContext::Leave(void)
{
    v8::HandleScope handle_scope(m_isolate);
    Handle()->Exit();
}

bool CContext::HasOutOfMemoryException(void)
{
    v8::HandleScope handle_scope(m_isolate);
    return Handle()->HasOutOfMemoryException();
}

CContext::CContext(const CContext& context)
{
  m_isolate = context.Handle()->GetIsolate();
  
  v8::HandleScope handle_scope(m_isolate);
  m_context.Reset(context.Handle()->GetIsolate(), context.Handle());
}

void CContext::Init(py::list extensions)
{
  v8::HandleScope handle_scope(m_isolate);

  std::auto_ptr<v8::ExtensionConfiguration> cfg;
  std::vector<std::string> ext_names;
  std::vector<const char *> ext_ptrs;

  for (Py_ssize_t i=0; i<PyList_Size(extensions.ptr()); i++)
  {
    py::extract<const std::string> extractor(::PyList_GetItem(extensions.ptr(), i));

    if (extractor.check())
    {
      ext_names.push_back(extractor());
    }
  }

  for (size_t i=0; i<ext_names.size(); i++)
  {
    ext_ptrs.push_back(ext_names[i].c_str());
  }

  if (!ext_ptrs.empty()) cfg.reset(new v8::ExtensionConfiguration(ext_ptrs.size(), &ext_ptrs[0]));

  v8::Handle<v8::Context> context = v8::Context::New(m_isolate, cfg.get());

  m_context.Reset(m_isolate, context);

  v8::Context::Scope context_scope(Handle());

  if (!m_global.is_none())
  {
    Handle()->Global()->Set(v8::String::NewFromUtf8(m_isolate, "__proto__"), CPythonObject::Wrap(m_global, m_isolate));
    Py_DECREF(m_global.ptr());
  }
}

py::object CContext::GetGlobal(void)
{
  v8::HandleScope handle_scope(m_isolate);

  return CJavascriptObject::Wrap(Handle()->Global(), m_isolate);
}

py::str CContext::GetSecurityToken(void)
{
  v8::HandleScope handle_scope(m_isolate);

  v8::Handle<v8::Value> token = Handle()->GetSecurityToken();

  if (token.IsEmpty()) return py::str();

  v8::String::Utf8Value str(token->ToString());

  return py::str(*str, str.length());
}

void CContext::SetSecurityToken(py::str token)
{
  v8::HandleScope handle_scope(m_isolate);

  if (token.is_none())
  {
    Handle()->UseDefaultSecurityToken();
  }
  else
  {
    Handle()->SetSecurityToken(v8::String::NewFromUtf8(m_isolate, py::extract<const char *>(token)()));
  }
}

py::object CContext::Evaluate(const std::string& src,
                              const std::string name,
                              int line, int col,
                              py::object precompiled)
{
  CEngine engine(m_isolate);

  CScriptPtr script = engine.Compile(src, name, line, col, precompiled);

  return script->Run();
}

py::object CContext::EvaluateW(const std::wstring& src,
                               const std::wstring name,
                               int line, int col,
                               py::object precompiled)
{
  CEngine engine(m_isolate);
  
  CScriptPtr script = engine.CompileW(src, name, line, col, precompiled);
  
  return script->Run();
}
