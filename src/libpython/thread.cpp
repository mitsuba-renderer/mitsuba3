#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include "python.h"

MTS_PY_EXPORT(Thread) {
    MTS_PY_CLASS(Thread, Object)
        .mdef(Thread, setPriority)
        .mdef(Thread, getPriority)
        .mdef(Thread, setCoreAffinity)
        .mdef(Thread, getCoreAffinity)
        .mdef(Thread, setCritical)
        .mdef(Thread, getCritical)
        .mdef(Thread, setName)
        .mdef(Thread, getName)
        .def("getParent", (Thread *(Thread::*)()) &Thread::getParent, DM(Thread, getParent))
        .mdef(Thread, getID)
        .mdef(Thread, getLogger)
        .mdef(Thread, getLogger)
        .sdef(Thread, getThread)
        .mdef(Thread, start)
        .mdef(Thread, isRunning)
        .mdef(Thread, detach)
        .mdef(Thread, join)
        .sdef(Thread, sleep);

    py::enum_<Thread::EPriority>(m.attr("Thread"), "EPriority")
        .value("EIdlePriority,", Thread::EIdlePriority)
        .value("ELowestPriority,", Thread::ELowestPriority)
        .value("ELowPriority,", Thread::ELowPriority)
        .value("ENormalPriority,", Thread::ENormalPriority)
        .value("EHighPriority,", Thread::EHighPriority)
        .value("EHighestPriority,", Thread::EHighestPriority)
        .value("ERealtimePriority", Thread::ERealtimePriority)
        .export_values();
}

