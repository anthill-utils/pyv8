#include "Locker.h"

#include "V8Internal.h"

bool CLocker::s_preemption = false;

CLocker::CLocker() : m_isolate(v8i::Isolate::GetDefaultIsolateForLocking()) {}
CLocker::CLocker(CIsolatePtr isolate) : m_isolate(isolate->GetIsolate()) {}

void CLocker::enter(void)
{
    Py_BEGIN_ALLOW_THREADS

    m_locker.reset(new v8::Locker(m_isolate));

    Py_END_ALLOW_THREADS
}

void CLocker::leave(void)
{
    Py_BEGIN_ALLOW_THREADS

    m_locker.reset();

    Py_END_ALLOW_THREADS
}

void CLocker::ResetActive()
{
    v8::Locker::ResetActive();
}

bool CLocker::IsLocked()
{
  return v8::Locker::IsLocked(m_isolate);
}

CUnlocker::CUnlocker() : m_isolate(v8i::Isolate::GetDefaultIsolateForLocking()) {}
CUnlocker::CUnlocker(CIsolatePtr isolate) : m_isolate(isolate->GetIsolate()) {}

void CLocker::Expose(void)
{
  py::class_<CLocker, boost::noncopyable>("JSLocker", py::no_init)
    .def(py::init<>())
    .def(py::init<CIsolatePtr>((py::arg("isolate"))))

    .add_static_property("active", &v8::Locker::IsActive,
                         "whether Locker is being used by this V8 instance.")

    .add_property("locked", &CLocker::IsLocked, "whether or not the locker is locked by the current thread.")

    .def("entered", &CLocker::entered)
    
    .def("resetActive", &CLocker::ResetActive)
    .staticmethod("resetActive")

    .def("enter", &CLocker::enter)
    .def("leave", &CLocker::leave)
    ;

  py::class_<CUnlocker, boost::noncopyable>("JSUnlocker", py::no_init)
    .def(py::init<>())
    .def(py::init<CIsolatePtr>((py::arg("isolate"))))
    
    .def("entered", &CUnlocker::entered)

    .def("enter", &CUnlocker::enter)
    .def("leave", &CUnlocker::leave)
    ;
}
