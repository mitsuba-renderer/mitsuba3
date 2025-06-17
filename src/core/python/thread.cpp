#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(Thread) {
    auto thr = nb::class_<Thread, Object>(m, "Thread", D(Thread));

    thr.def(nb::init<>())
       .def_static("file_resolver", []() -> FileResolver* {
           PyErr_WarnEx(PyExc_DeprecationWarning, "Thread.file_resolver() is deprecated, please use mi.file_resolver()", 1);
           return Thread::file_resolver();
       }, nb::rv_policy::reference, D(Thread, file_resolver))
       .def_static("logger", []() -> Logger* {
           PyErr_WarnEx(PyExc_DeprecationWarning, "Thread.logger() is deprecated, please use mi.logger()", 1);
           return Thread::logger();
       }, nb::rv_policy::reference, D(Thread, logger))
       .def_static("set_logger", &set_logger, "logger"_a)
       .def_static("set_file_resolver", &set_file_resolver)
       .def_static("thread", &Thread::thread,
           nb::rv_policy::reference)
       .def_static("wait_for_tasks", &Thread::wait_for_tasks);
}
