#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/jit.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/fresolver.h>
#include "python.h"

// ext
MTS_PY_DECLARE(pcg32);

// libmitsuba-core
MTS_PY_DECLARE(filesystem);
MTS_PY_DECLARE(atomic);
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

// libmitsuba-render
MTS_PY_DECLARE(Scene);
MTS_PY_DECLARE(Shape);
MTS_PY_DECLARE(ShapeKDTree);

// libmitsuba-ui
//MTS_PY_DECLARE(WarpVisualizationWidget);

PYBIND11_PLUGIN(mitsuba) {
    Jit::staticInitialization();
    Class::staticInitialization();
    Thread::staticInitialization();
    Logger::staticInitialization();

    py::module m("mitsuba", "Mitsuba Python extension library");
    py::module core = m.def_submodule("core",
        "Mitsuba core support library (generic mathematical and I/O routines)");
    py::module render = m.def_submodule("render",
        "Mitsuba rendering support library (scene representation, ray intersection, ...)");
    py::module ui = m.def_submodule("ui",
        "Mitsuba user interface library (rendering interface, visualization of warping functions, ...)");

    // ext
    MTS_PY_IMPORT_CORE(pcg32);

    // libmitsuba-core
    MTS_PY_IMPORT_CORE(filesystem);
    MTS_PY_IMPORT_CORE(atomic);
    MTS_PY_IMPORT_CORE(util);
    MTS_PY_IMPORT_CORE(math);
    MTS_PY_IMPORT_CORE(xml);
    MTS_PY_IMPORT_CORE(vector);
    MTS_PY_IMPORT_CORE(Object);
    MTS_PY_IMPORT_CORE(Thread);
    MTS_PY_IMPORT_CORE(Logger);
    MTS_PY_IMPORT_CORE(Appender);
    MTS_PY_IMPORT_CORE(Formatter);
    MTS_PY_IMPORT_CORE(Properties);
    MTS_PY_IMPORT_CORE(ArgParser);
    MTS_PY_IMPORT_CORE(FileResolver);
    MTS_PY_IMPORT_CORE(Stream);
    MTS_PY_IMPORT_CORE(AnnotatedStream);
    MTS_PY_IMPORT_CORE(DummyStream);
    MTS_PY_IMPORT_CORE(FileStream);
    MTS_PY_IMPORT_CORE(MemoryStream);
    MTS_PY_IMPORT_CORE(ZStream);
    MTS_PY_IMPORT_CORE(BoundingBox);
    MTS_PY_IMPORT_CORE(Ray);
    MTS_PY_IMPORT_CORE(Frame);
    MTS_PY_IMPORT_CORE(Struct);
    MTS_PY_IMPORT_CORE(Bitmap);
    MTS_PY_IMPORT_CORE(warp);

    // libmitsuba-render
    MTS_PY_IMPORT_RENDER(Scene);
    MTS_PY_IMPORT_RENDER(Shape);
    MTS_PY_IMPORT_RENDER(ShapeKDTree);

    // libmitsuba-ui
    //MTS_PY_IMPORT_UI(WarpVisualizationWidget);

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

    return m.ptr();
}
