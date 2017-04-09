#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/jit.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/simd.h>
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
MTS_PY_DECLARE(Transform);
MTS_PY_DECLARE(Struct);
MTS_PY_DECLARE(Bitmap);
MTS_PY_DECLARE(Spectrum);
MTS_PY_DECLARE(warp);
MTS_PY_DECLARE(qmc);
MTS_PY_DECLARE(spline);
MTS_PY_DECLARE(DiscreteDistribution);

PYBIND11_PLUGIN(mitsuba_core_ext) {
    // Expose some constants in the main `mitsuba` module
    py::module m_parent = py::module::import("mitsuba");
    m_parent.attr("__version__") = MTS_VERSION;
    m_parent.attr("MTS_VERSION") = MTS_VERSION;
    m_parent.attr("MTS_YEAR")    = MTS_YEAR;
    m_parent.attr("MTS_AUTHORS") = MTS_AUTHORS;

#if defined(NDEBUG)
    m_parent.attr("DEBUG")  = false;
    m_parent.attr("NDEBUG") = true;
#else    // NDEBUG
    m_parent.attr("DEBUG")  = true;
    m_parent.attr("NDEBUG") = false;
#endif   // NDEBUG

    // Import submodules of `mitsuba.core`
    py::module m_("mitsuba_core_ext"); // unused
    py::module m = py::module::import("mitsuba.core");

#if defined(SINGLE_PRECISION)
    m.attr("float_dtype") = py::dtype("f");
#else
    m.attr("float_dtype") = py::dtype("d");
#endif
    m.attr("PacketSize") = py::cast(PacketSize);

    Jit::static_initialization();
    Class::static_initialization();
    Thread::static_initialization();
    Logger::static_initialization();

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
    MTS_PY_IMPORT(Transform);
    MTS_PY_IMPORT(Struct);
    MTS_PY_IMPORT(Bitmap);
    MTS_PY_IMPORT(Spectrum);
    MTS_PY_IMPORT(warp);
    MTS_PY_IMPORT(qmc);
    MTS_PY_IMPORT(spline);
    MTS_PY_IMPORT(DiscreteDistribution);

    auto cleanup_callback = []() {
        Logger::static_shutdown();
        Thread::static_shutdown();
        Class::static_shutdown();
        Jit::static_shutdown();
    };

    m.add_object("_cleanup", py::capsule(cleanup_callback));

    /* Append the mitsuba directory to the FileResolver search path list */
    ref<FileResolver> fr = Thread::thread()->file_resolver();
    fs::path basePath = util::library_path().parent_path();
    if (!fr->contains(basePath))
        fr->append(basePath);

    return m_.ptr();
}
