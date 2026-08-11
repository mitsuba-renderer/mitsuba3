#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(Thread) {
    auto thr = nb::class_<Thread, Object>(m, "Thread", D(Thread));

    thr.def(nb::init<>())
       .def_static("set_logger", &set_logger, "logger"_a)
       .def_static("set_file_resolver", &set_file_resolver)
       .def_static("thread", &Thread::thread,
           nb::rv_policy::reference)
       .def_static("wait_for_tasks", &Thread::wait_for_tasks,
           nb::call_guard<nb::gil_scoped_release>(), D(Thread, wait_for_tasks));
}
