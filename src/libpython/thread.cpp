#include "python.h"
#include <mitsuba/core/thread.h>

MTS_PY_EXPORT(Thread) {
    py::class_<Thread, ref<Thread>> thrd(m, "Thread", DM(Thread));

    thrd.def("setPriority", &Thread::setPriority, DM(Thread, getPriority))
        .def("getPriority", &Thread::getPriority, DM(Thread, setPriority))
        .def("setCoreAffinity", &Thread::setCoreAffinity, DM(Thread, getCoreAffinity))
        .def("getCoreAffinity", &Thread::getCoreAffinity, DM(Thread, setCoreAffinity));

	py::enum_<Thread::EPriority>(thrd, "EPriority")
        .value("EIdlePriority,", Thread::EIdlePriority)
        .value("ELowestPriority,", Thread::ELowestPriority)
        .value("ELowPriority,", Thread::ELowPriority)
        .value("ENormalPriority,", Thread::ENormalPriority)
        .value("EHighPriority,", Thread::EHighPriority)
        .value("EHighestPriority,", Thread::EHighestPriority)
        .value("ERealtimePriority", Thread::ERealtimePriority)
        .export_values();
}

