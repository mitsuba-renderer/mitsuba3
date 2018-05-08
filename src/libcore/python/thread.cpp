#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/python/python.h>

/**
 * Trampoline class for mitsuba::Thread, allows defining custom threads through
 * inheritance from Python.
 */
class PyThread : public Thread {
public:
    using Thread::Thread;
    virtual ~PyThread() = default;

    std::string to_string() const override {
        py::gil_scoped_acquire acquire;
        PYBIND11_OVERLOAD(std::string, Thread, to_string);
    }

protected:
    void run() override {
        py::gil_scoped_acquire acquire;
        PYBIND11_OVERLOAD_PURE(void, Thread, run);
    }
};

MTS_PY_EXPORT(Thread) {
    auto th = py::class_<Thread, Object, ref<Thread>, PyThread>(m, "Thread", D(Thread))
        .def(py::init<const std::string &>(), "name"_a)
        .def("parent", (Thread * (Thread::*) ()) & Thread::parent,
             D(Thread, parent))
        .def("file_resolver",
             (FileResolver * (Thread::*) ()) & Thread::file_resolver,
             D(Thread, file_resolver))
        .mdef(Thread, set_priority)
        .mdef(Thread, priority)
        .mdef(Thread, set_core_affinity)
        .mdef(Thread, core_affinity)
        .mdef(Thread, set_critical)
        .mdef(Thread, is_critical)
        .mdef(Thread, set_name)
        .mdef(Thread, name)
        .mdef(Thread, id)
        .mdef(Thread, logger)
        .mdef(Thread, set_logger)
        .mdef(Thread, set_file_resolver)
        .sdef(Thread, thread)
        .mdef(Thread, start)
        .mdef(Thread, is_running)
        .mdef(Thread, detach)
        .mdef(Thread, join)
        .sdef(Thread, sleep);

    py::enum_<Thread::EPriority>(th, "EPriority", D(Thread, EPriority))
        .value("EIdlePriority", Thread::EIdlePriority, D(Thread, EPriority, EIdlePriority))
        .value("ELowestPriority", Thread::ELowestPriority, D(Thread, EPriority, ELowestPriority))
        .value("ELowPriority", Thread::ELowPriority, D(Thread, EPriority, ELowPriority))
        .value("ENormalPriority", Thread::ENormalPriority, D(Thread, EPriority, ENormalPriority))
        .value("EHighPriority", Thread::EHighPriority, D(Thread, EPriority, EHighPriority))
        .value("EHighestPriority", Thread::EHighestPriority, D(Thread, EPriority, EHighestPriority))
        .value("ERealtimePriority", Thread::ERealtimePriority, D(Thread, EPriority, ERealtimePriority))
        .export_values();
}

