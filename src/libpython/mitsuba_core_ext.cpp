#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/jit.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/fresolver.h>
#include "python.h"

// libmitsuba-core
MTS_PY_DECLARE(filesystem);
MTS_PY_DECLARE(atomic);
MTS_PY_DECLARE(random);
MTS_PY_DECLARE(util);
MTS_PY_DECLARE(math);
MTS_PY_DECLARE(xml);
MTS_PY_DECLARE(vector);
MTS_PY_DECLARE(Object);
MTS_PY_DECLARE(Thread);
MTS_PY_DECLARE(Logger);
MTS_PY_DECLARE(Appender);
MTS_PY_DECLARE(Formatter);
MTS_PY_DECLARE(Properties);
MTS_PY_DECLARE(ArgParser);
MTS_PY_DECLARE(FileResolver);
MTS_PY_DECLARE(Stream);
MTS_PY_DECLARE(AnnotatedStream);
MTS_PY_DECLARE(DummyStream);
MTS_PY_DECLARE(FileStream);
MTS_PY_DECLARE(MemoryStream);
MTS_PY_DECLARE(ZStream);
MTS_PY_DECLARE(BoundingBox);
MTS_PY_DECLARE(Ray);
MTS_PY_DECLARE(Frame);
MTS_PY_DECLARE(Struct);
MTS_PY_DECLARE(Bitmap);
MTS_PY_DECLARE(warp);
MTS_PY_DECLARE(qmc);

PYBIND11_PLUGIN(mitsuba_core_ext) {
    py::module m_("mitsuba_core_ext"); // unused
    py::module m = py::module::import("mitsuba.core");

    Jit::staticInitialization();
    Class::staticInitialization();
    Thread::staticInitialization();
    Logger::staticInitialization();

    // libmitsuba-core
    MTS_PY_IMPORT(filesystem);
    MTS_PY_IMPORT(atomic);
    MTS_PY_IMPORT(random);
    MTS_PY_IMPORT(util);
    MTS_PY_IMPORT(math);
    MTS_PY_IMPORT(xml);
    MTS_PY_IMPORT(vector);
    MTS_PY_IMPORT(Object);
    MTS_PY_IMPORT(Thread);
    MTS_PY_IMPORT(Logger);
    MTS_PY_IMPORT(Appender);
    MTS_PY_IMPORT(Formatter);
    MTS_PY_IMPORT(Properties);
    MTS_PY_IMPORT(ArgParser);
    MTS_PY_IMPORT(FileResolver);
    MTS_PY_IMPORT(Stream);
    MTS_PY_IMPORT(AnnotatedStream);
    MTS_PY_IMPORT(DummyStream);
    MTS_PY_IMPORT(FileStream);
    MTS_PY_IMPORT(MemoryStream);
    MTS_PY_IMPORT(ZStream);
    MTS_PY_IMPORT(BoundingBox);
    MTS_PY_IMPORT(Ray);
    MTS_PY_IMPORT(Frame);
    MTS_PY_IMPORT(Struct);
    MTS_PY_IMPORT(Bitmap);
    MTS_PY_IMPORT(warp);
    MTS_PY_IMPORT(qmc);

    atexit([]() {
        Logger::staticShutdown();
        Thread::staticShutdown();
        Class::staticShutdown();
        Jit::staticShutdown();
    });

    /* Append the mitsuba directory to the FileResolver search path list */
    ref<FileResolver> fr = Thread::thread()->fileResolver();
    fs::path basePath = util::libraryPath().parent_path();
    if (!fr->contains(basePath))
        fr->append(basePath);

    return m_.ptr();
}
