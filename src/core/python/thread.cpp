#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/python/python.h>

#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Trampoline class for mitsuba::Thread, allows defining custom threads through
 * inheritance from Python.
 */
class PyThread : public Thread {
public:
    NB_TRAMPOLINE(Thread, 2);

    virtual ~PyThread() = default;

    std::string to_string() const override {
        nb::gil_scoped_acquire acquire;
        NB_OVERRIDE(to_string);
    }

protected:
    void run() override {
        nb::gil_scoped_acquire acquire;
        NB_OVERRIDE_PURE(run);
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


    void exit(nb::handle, nb::handle, nb::handle) {
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

/// Dummy object that clears the thread local state upon deletion
class ThreadCanary {
public:
    ThreadCanary() { }

    ~ThreadCanary() {
        if (Thread::unregister_external_thread())
            Thread::tls_shutdown();
    }
};

MI_PY_EXPORT(Thread) {
    auto thr = nb::class_<Thread, Object, PyThread>(m, "Thread", D(Thread));

    nb::enum_<Thread::EPriority>(thr, "EPriority", D(Thread, EPriority))
        .value("EIdlePriority", Thread::EIdlePriority, D(Thread, EPriority, EIdlePriority))
        .value("ELowestPriority", Thread::ELowestPriority, D(Thread, EPriority, ELowestPriority))
        .value("ELowPriority", Thread::ELowPriority, D(Thread, EPriority, ELowPriority))
        .value("ENormalPriority", Thread::ENormalPriority, D(Thread, EPriority, ENormalPriority))
        .value("EHighPriority", Thread::EHighPriority, D(Thread, EPriority, EHighPriority))
        .value("EHighestPriority", Thread::EHighestPriority, D(Thread, EPriority, EHighestPriority))
        .value("ERealtimePriority", Thread::ERealtimePriority, D(Thread, EPriority, ERealtimePriority))
        .export_values();

    nb::class_<ThreadCanary>(m, "_ThreadCanary").def(nb::init<>());

    thr.def(nb::init<const std::string &>(), "name"_a)
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
       .def_static_method(Thread, thread_id)
       .def_method(Thread, logger)
       .def_method(Thread, set_logger)
       .def_method(Thread, set_file_resolver)
       .def_static("thread",
        [&]() {
            /* The thread local information stored by `self` only gets freed
             * when the TLS destructor is called. This is problematic as Python
             * does not wait for the TLS destrcutors to be finished before
             * shutting down the interpreter - even if some of those
             * destructors require the interpreter to be alive. Ultimately,
             * this can lead to leaks or crashes.
             *
             * Below, we create a canary object that will clear the `self`
             * variable whenever it is deleted and we attach it to the Python
             * `thread.Thread` that called this method. Functionally, we're
             * tying the lifetime of the `self` variable to the Python
             * `thread.Thread` object and therefore forcing Python to wait on
             * the TLS data to be cleared. The TLS destructors will still be
             * called but they will no longer require access to the Python
             * interpreter as `self` will be empty. */

            nb::object threading = nb::module_::import_("threading");
            nb::object current_thread = threading.attr("current_thread")();

            // Only do this the first time
            if (!nb::hasattr(current_thread, "mitsuba_tls")) {
                nb::object tls = threading.attr("local")();
                nb::object mitsuba = nb::module_::import_("mitsuba");
                nb::object canary = mitsuba.attr("_ThreadCanary")();

                // Attach canary to Python thread.
                tls.attr("mitsuba_canary") = canary;
                current_thread.attr("mitsuba_tls") = tls;
            }

            return Thread::thread();
        }, D(Thread, thread))
       .def_static_method(Thread, register_external_thread)
       .def_method(Thread, start)
       .def_method(Thread, is_running)
       .def_method(Thread, detach)
       .def_method(Thread, join)
       .def_static_method(Thread, sleep)
       .def_static_method(Thread, wait_for_tasks);

    nb::class_<ThreadEnvironment>(m, "ThreadEnvironment", D(ThreadEnvironment))
        .def(nb::init<>());

    nb::class_<PyScopedSetThreadEnvironment>(m, "ScopedSetThreadEnvironment",
                                            D(ScopedSetThreadEnvironment))
        .def(nb::init<const ThreadEnvironment &>())
        .def("__enter__", &PyScopedSetThreadEnvironment::enter)
        .def("__exit__", &PyScopedSetThreadEnvironment::exit,
             "exc_type"_a.none(), "exc_val"_a.none(), "exc_tb"_a.none());
}

