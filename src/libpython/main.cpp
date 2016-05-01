#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include "python.h"

MTS_PY_DECLARE(pcg32);
MTS_PY_DECLARE(Object);
MTS_PY_DECLARE(Thread);
MTS_PY_DECLARE(Logger);
MTS_PY_DECLARE(Appender);
MTS_PY_DECLARE(Formatter);
MTS_PY_DECLARE(atomic);

PYBIND11_PLUGIN(mitsuba) {
    Class::staticInitialization();
    Thread::staticInitialization();
    Logger::staticInitialization();

    py::module m("mitsuba", "Mitsuba Python plugin");

    MTS_PY_IMPORT(pcg32);
    MTS_PY_IMPORT(Object);
    MTS_PY_IMPORT(Thread);
    MTS_PY_IMPORT(Logger);
    MTS_PY_IMPORT(Appender);
    MTS_PY_IMPORT(Formatter);
    MTS_PY_IMPORT(atomic);

    atexit([](){
        Logger::staticShutdown();
        Thread::staticShutdown();
        Class::staticShutdown();
    });

    return m.ptr();
}
