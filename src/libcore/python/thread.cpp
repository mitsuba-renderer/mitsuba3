#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/tls.h>
#include <mitsuba/python/python.h>

NAMESPACE_BEGIN(mitsuba)

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

class PyScopedSetThreadEnvironment {
public:
    PyScopedSetThreadEnvironment(const ThreadEnvironment &env) : m_env(env) { }

    void enter() {
        if (m_ste)
            return;

        m_registered = Thread::register_external_thread("py");
        m_ste = new ScopedSetThreadEnvironment(m_env);
    }


    void exit(py::handle, py::handle, py::handle) {
        if (!m_ste)
            return;

        delete m_ste;
        m_ste = nullptr;

        if (m_registered) {
            Thread::unregister_external_thread();
            m_registered = false;
        }
    }

    ThreadEnvironment m_env;
    ScopedSetThreadEnvironment *m_ste = nullptr;
    bool m_registered = false;
};

NAMESPACE_END(mitsuba)

MTS_PY_EXPORT(Thread) {
    auto thr = py::class_<Thread, Object, ref<Thread>, PyThread>(m, "Thread", D(Thread));

    py::enum_<Thread::EPriority>(thr, "EPriority", D(Thread, EPriority))
        .value("EIdlePriority", Thread::EIdlePriority, D(Thread, EPriority, EIdlePriority))
        .value("ELowestPriority", Thread::ELowestPriority, D(Thread, EPriority, ELowestPriority))
        .value("ELowPriority", Thread::ELowPriority, D(Thread, EPriority, ELowPriority))
        .value("ENormalPriority", Thread::ENormalPriority, D(Thread, EPriority, ENormalPriority))
        .value("EHighPriority", Thread::EHighPriority, D(Thread, EPriority, EHighPriority))
        .value("EHighestPriority", Thread::EHighestPriority, D(Thread, EPriority, EHighestPriority))
        .value("ERealtimePriority", Thread::ERealtimePriority, D(Thread, EPriority, ERealtimePriority))
        .export_values();


    thr.def(py::init<const std::string &>(), "name"_a)
       .def("parent", (Thread * (Thread::*) ()) & Thread::parent,
           D(Thread, parent))
       .def("file_resolver",
           (FileResolver * (Thread::*) ()) & Thread::file_resolver,
           D(Thread, file_resolver))
       .def_method(Thread, set_priority)
       .def_method(Thread, priority)
       .def_method(Thread, set_core_affinity)
       .def_method(Thread, core_affinity)
       .def_method(Thread, set_critical)
       .def_method(Thread, is_critical)
       .def_method(Thread, set_name)
       .def_method(Thread, name)
       .def_method(Thread, thread_id)
       .def_method(Thread, logger)
       .def_method(Thread, set_logger)
       .def_method(Thread, set_file_resolver)
       .def_static_method(Thread, thread)
       .def_static_method(Thread, register_external_thread)
       .def_method(Thread, start)
       .def_method(Thread, is_running)
       .def_method(Thread, detach)
       .def_method(Thread, join)
       .def_static_method(Thread, sleep);

    py::class_<ThreadEnvironment>(m, "ThreadEnvironment", D(ThreadEnvironment))
        .def(py::init<>());

    py::class_<PyScopedSetThreadEnvironment>(m, "ScopedSetThreadEnvironment",
                                            D(ScopedSetThreadEnvironment))
        .def(py::init<const ThreadEnvironment &>())
        .def("__enter__", &PyScopedSetThreadEnvironment::enter)
        .def("__exit__", &PyScopedSetThreadEnvironment::exit);
}

