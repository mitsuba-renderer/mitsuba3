#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/fresolver.h>
#include "python.h"

MTS_PY_EXPORT(Thread) {
    MTS_PY_CLASS(Thread, Object)
        .def("parent", (Thread *(Thread::*)()) &Thread::parent, DM(Thread, parent))
        .def("fileResolver", (FileResolver *(Thread::*)()) &Thread::fileResolver, DM(Thread, fileResolver))
        .mdef(Thread, setPriority)
        .mdef(Thread, priority)
        .mdef(Thread, setCoreAffinity)
        .mdef(Thread, coreAffinity)
        .mdef(Thread, setCritical)
        .mdef(Thread, isCritical)
        .mdef(Thread, setName)
        .mdef(Thread, name)
        .mdef(Thread, id)
        .mdef(Thread, logger)
        .mdef(Thread, setLogger)
        .mdef(Thread, setFileResolver)
        .sdef(Thread, thread)
        .mdef(Thread, start)
        .mdef(Thread, isRunning)
        .mdef(Thread, detach)
        .mdef(Thread, join)
        .sdef(Thread, sleep);

    py::enum_<Thread::EPriority>(m.attr("Thread"), "EPriority", DM(Thread, EPriority))
        .value("EIdlePriority,", Thread::EIdlePriority)
        .value("ELowestPriority,", Thread::ELowestPriority)
        .value("ELowPriority,", Thread::ELowPriority)
        .value("ENormalPriority,", Thread::ENormalPriority)
        .value("EHighPriority,", Thread::EHighPriority)
        .value("EHighestPriority,", Thread::EHighestPriority)
        .value("ERealtimePriority", Thread::ERealtimePriority)
        .export_values();
}

