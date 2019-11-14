#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/jit.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/simd.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/util.h>
#include <mitsuba/python/python.h>
#include <tbb/task_scheduler_init.h>

MTS_PY_DECLARE(filesystem);
MTS_PY_DECLARE(atomic);
MTS_PY_DECLARE(autodiff);
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
MTS_PY_DECLARE(DummyStream);
MTS_PY_DECLARE(FileStream);
MTS_PY_DECLARE(MemoryStream);
MTS_PY_DECLARE(ZStream);
MTS_PY_DECLARE(BoundingBox);
MTS_PY_DECLARE(BoundingSphere);
MTS_PY_DECLARE(Ray);
MTS_PY_DECLARE(Frame);
MTS_PY_DECLARE(Transform);
MTS_PY_DECLARE(Struct);
MTS_PY_DECLARE(rfilter);
MTS_PY_DECLARE(Bitmap);
MTS_PY_DECLARE(Spectrum);
MTS_PY_DECLARE(warp);
MTS_PY_DECLARE(qmc);
MTS_PY_DECLARE(spline);
MTS_PY_DECLARE(quad);
MTS_PY_DECLARE(DiscreteDistribution);
MTS_PY_DECLARE(AnimatedTransform);
MTS_PY_DECLARE(MemoryMappedFile);
MTS_PY_DECLARE(ProgressReporter);

PYBIND11_MODULE(mitsuba_core_ext, m_) {
    (void) m_; /* unused */

    // Expose some constants in the main `mitsuba` module
    py::module m = py::module::import("mitsuba");
    m.attr("__version__")       = MTS_VERSION;
    m.attr("MTS_VERSION")       = MTS_VERSION;
    m.attr("MTS_VERSION_MAJOR") = MTS_VERSION_MAJOR;
    m.attr("MTS_VERSION_MINOR") = MTS_VERSION_MINOR;
    m.attr("MTS_VERSION_PATCH") = MTS_VERSION_PATCH;
    m.attr("MTS_YEAR")          = MTS_YEAR;
    m.attr("MTS_AUTHORS")       = MTS_AUTHORS;

#if defined(NDEBUG)
    m.attr("DEBUG")  = false;
    m.attr("NDEBUG") = true;
#else  // NDEBUG
    m.attr("DEBUG")  = true;
    m.attr("NDEBUG") = false;
#endif // NDEBUG

#if defined(MTS_ENABLE_AUTODIFF)
    m.attr("ENABLE_AUTODIFF")  = true;
#else
    m.attr("ENABLE_AUTODIFF")  = false;
#endif
#if defined(MTS_USE_EMBREE)
    m.attr("USE_EMBREE")  = true;
#else
    m.attr("USE_EMBREE")  = false;
#endif
#if defined(MTS_USE_OPTIX)
    m.attr("USE_OPTIX")  = true;
#else
    m.attr("USE_OPTIX")  = false;
#endif

    // Define submodules of `mitsuba.mode.core`
    MTS_PY_DEF_SUBMODULE(core)

// TODO should add this for all the different variants
// #if defined(SINGLE_PRECISION)
    m.attr("float_dtype") = py::dtype("f");
// #else
//     m.attr("float_dtype")   = py::dtype("d");
// #endif
//     m.attr("PacketSize") = py::cast(PacketSize);

    Jit::static_initialization();
    Class::static_initialization();
    Thread::static_initialization();
    Logger::static_initialization();
    Bitmap::static_initialization();

    // libmitsuba-core
    MTS_PY_IMPORT(filesystem);
    MTS_PY_IMPORT(atomic);
    MTS_PY_IMPORT(autodiff);
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
    MTS_PY_IMPORT(DummyStream);
    MTS_PY_IMPORT(FileStream);
    MTS_PY_IMPORT(MemoryStream);
    MTS_PY_IMPORT(ZStream);
    MTS_PY_IMPORT(Ray);
    MTS_PY_IMPORT(BoundingBox);
    MTS_PY_IMPORT(BoundingSphere);
    MTS_PY_IMPORT(Frame);
    MTS_PY_IMPORT(Transform);
    MTS_PY_IMPORT(Struct);
    MTS_PY_IMPORT(rfilter);
    MTS_PY_IMPORT(Bitmap);
    MTS_PY_IMPORT(Spectrum);
    MTS_PY_IMPORT(warp);
    MTS_PY_IMPORT(qmc);
    MTS_PY_IMPORT(spline);
    MTS_PY_IMPORT(quad);
    MTS_PY_IMPORT(DiscreteDistribution);
    MTS_PY_IMPORT(AnimatedTransform);
    MTS_PY_IMPORT(MemoryMappedFile);
    MTS_PY_IMPORT(ProgressReporter);

    /* Append the mitsuba directory to the FileResolver search path list */
    ref<FileResolver> fr = Thread::thread()->file_resolver();
    fs::path basePath    = util::library_path().parent_path();
    if (!fr->contains(basePath))
        fr->append(basePath);

    /**
     * Holds a pointer to the current TBB task scheduler.
     * This will allow us to replace it as needed if we'd like to change the
     * thread count from the Python side.
     * The holder itself is heap-allocated so that it survives until we manually
     * delete it in \ref cleanup_callback.
     */
    auto *scheduler_holder = new std::unique_ptr<tbb::task_scheduler_init>(
        new tbb::task_scheduler_init());
    m.def(
        "set_thread_count",
        [scheduler_holder](int count) {
            // Make sure the previous scheduler is deleted first.
            scheduler_holder->reset(nullptr);
            scheduler_holder->reset(new tbb::task_scheduler_init(count));
            __global_thread_count = count;
        },
        "count"_a = -1,
        "Sets the maximum number of threads to be used by TBB. Defaults to "
        "-1 (automatic).");

    // TODO enable this
    /* Register a cleanup callback function that is invoked when
       the 'mitsuba::Object' Python type is garbage collected */
    // py::cpp_function cleanup_callback([scheduler_holder](py::handle weakref) {
    //     delete scheduler_holder;

    //     Bitmap::static_shutdown();
    //     Logger::static_shutdown();
    //     Thread::static_shutdown();
    //     Class::static_shutdown();
    //     Jit::static_shutdown();
    //     weakref.dec_ref();
    // });

    // (void) py::weakref(m.attr("Object"), cleanup_callback).release();
}
