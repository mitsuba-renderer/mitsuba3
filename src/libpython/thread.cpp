#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/fresolver.h>
#include "python.h"

MTS_PY_EXPORT(Thread) {
    MTS_PY_CLASS(Thread, Object)
        .def("parent", (Thread * (Thread::*) ()) & Thread::parent,
             D(Thread, parent))
        .def("fileResolver",
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

    py::enum_<Thread::EPriority>(m.attr("Thread"), "EPriority", D(Thread, EPriority))
        .value("EIdlePriority,", Thread::EIdlePriority)
        .value("ELowestPriority,", Thread::ELowestPriority)
        .value("ELowPriority,", Thread::ELowPriority)
        .value("ENormalPriority,", Thread::ENormalPriority)
        .value("EHighPriority,", Thread::EHighPriority)
        .value("EHighestPriority,", Thread::EHighestPriority)
        .value("ERealtimePriority", Thread::ERealtimePriority)
        .export_values();
}

