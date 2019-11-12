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

MTS_PY_DECLARE_VARIANTS(filesystem);
MTS_PY_DECLARE_VARIANTS(atomic);
MTS_PY_DECLARE_VARIANTS(autodiff);
MTS_PY_DECLARE_VARIANTS(random);
MTS_PY_DECLARE_VARIANTS(util);
MTS_PY_DECLARE_VARIANTS(math);
MTS_PY_DECLARE_VARIANTS(xml);
MTS_PY_DECLARE_VARIANTS(vector);
MTS_PY_DECLARE_VARIANTS(Object);
MTS_PY_DECLARE_VARIANTS(Thread);
MTS_PY_DECLARE_VARIANTS(Logger);
MTS_PY_DECLARE_VARIANTS(Appender);
MTS_PY_DECLARE_VARIANTS(Formatter);
MTS_PY_DECLARE_VARIANTS(Properties);
MTS_PY_DECLARE_VARIANTS(ArgParser);
MTS_PY_DECLARE_VARIANTS(FileResolver);
MTS_PY_DECLARE_VARIANTS(Stream);
MTS_PY_DECLARE_VARIANTS(DummyStream);
MTS_PY_DECLARE_VARIANTS(FileStream);
MTS_PY_DECLARE_VARIANTS(MemoryStream);
MTS_PY_DECLARE_VARIANTS(ZStream);
MTS_PY_DECLARE_VARIANTS(BoundingBox);
MTS_PY_DECLARE_VARIANTS(BoundingSphere);
MTS_PY_DECLARE_VARIANTS(Ray);
MTS_PY_DECLARE_VARIANTS(Frame);
MTS_PY_DECLARE_VARIANTS(Transform);
MTS_PY_DECLARE_VARIANTS(Struct);
MTS_PY_DECLARE_VARIANTS(rfilter);
MTS_PY_DECLARE_VARIANTS(Bitmap);
MTS_PY_DECLARE_VARIANTS(Spectrum);
MTS_PY_DECLARE_VARIANTS(warp);
MTS_PY_DECLARE_VARIANTS(qmc);
MTS_PY_DECLARE_VARIANTS(spline);
MTS_PY_DECLARE_VARIANTS(quad);
MTS_PY_DECLARE_VARIANTS(DiscreteDistribution);
MTS_PY_DECLARE_VARIANTS(AnimatedTransform);
MTS_PY_DECLARE_VARIANTS(MemoryMappedFile);
MTS_PY_DECLARE_VARIANTS(ProgressReporter);

PYBIND11_MODULE(mitsuba_core_ext, m_) {
    (void) m_; /* unused */

    // Expose some constants in the main `mitsuba` module
    py::module m_parent = py::module::import("mitsuba");
    m_parent.attr("__version__")       = MTS_VERSION;
    m_parent.attr("MTS_VERSION")       = MTS_VERSION;
    m_parent.attr("MTS_VERSION_MAJOR") = MTS_VERSION_MAJOR;
    m_parent.attr("MTS_VERSION_MINOR") = MTS_VERSION_MINOR;
    m_parent.attr("MTS_VERSION_PATCH") = MTS_VERSION_PATCH;
    m_parent.attr("MTS_YEAR")          = MTS_YEAR;
    m_parent.attr("MTS_AUTHORS")       = MTS_AUTHORS;

#if defined(NDEBUG)
    m_parent.attr("DEBUG")  = false;
    m_parent.attr("NDEBUG") = true;
#else  // NDEBUG
    m_parent.attr("DEBUG")  = true;
    m_parent.attr("NDEBUG") = false;
#endif // NDEBUG

#if defined(MTS_ENABLE_AUTODIFF)
    m_parent.attr("ENABLE_AUTODIFF")  = true;
#else
    m_parent.attr("ENABLE_AUTODIFF")  = false;
#endif
#if defined(MTS_USE_EMBREE)
    m_parent.attr("USE_EMBREE")  = true;
#else
    m_parent.attr("USE_EMBREE")  = false;
#endif
#if defined(MTS_USE_OPTIX)
    m_parent.attr("USE_OPTIX")  = true;
#else
    m_parent.attr("USE_OPTIX")  = false;
#endif

    // Import submodules of `mitsuba.core`
    py::module m = py::module::import("mitsuba");
    MTS_PY_DEF_SUBMODULE_VARIANTS(core)

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
    MTS_PY_IMPORT_VARIANTS(filesystem);
    MTS_PY_IMPORT_VARIANTS(atomic);
    MTS_PY_IMPORT_VARIANTS(autodiff);
    MTS_PY_IMPORT_VARIANTS(random);
    MTS_PY_IMPORT_VARIANTS(util);
    MTS_PY_IMPORT_VARIANTS(math);
    MTS_PY_IMPORT_VARIANTS(xml);
    MTS_PY_IMPORT_VARIANTS(vector);
    MTS_PY_IMPORT_VARIANTS(Object);
    MTS_PY_IMPORT_VARIANTS(Thread);
    MTS_PY_IMPORT_VARIANTS(Logger);
    MTS_PY_IMPORT_VARIANTS(Appender);
    MTS_PY_IMPORT_VARIANTS(Formatter);
    MTS_PY_IMPORT_VARIANTS(Properties);
    MTS_PY_IMPORT_VARIANTS(ArgParser);
    MTS_PY_IMPORT_VARIANTS(FileResolver);
    MTS_PY_IMPORT_VARIANTS(Stream);
    MTS_PY_IMPORT_VARIANTS(DummyStream);
    MTS_PY_IMPORT_VARIANTS(FileStream);
    MTS_PY_IMPORT_VARIANTS(MemoryStream);
    MTS_PY_IMPORT_VARIANTS(ZStream);
    MTS_PY_IMPORT_VARIANTS(Ray);
    MTS_PY_IMPORT_VARIANTS(BoundingBox);
    MTS_PY_IMPORT_VARIANTS(BoundingSphere);
    MTS_PY_IMPORT_VARIANTS(Frame);
    MTS_PY_IMPORT_VARIANTS(Transform);
    MTS_PY_IMPORT_VARIANTS(Struct);
    MTS_PY_IMPORT_VARIANTS(rfilter);
    MTS_PY_IMPORT_VARIANTS(Bitmap);
    MTS_PY_IMPORT_VARIANTS(Spectrum);
    MTS_PY_IMPORT_VARIANTS(warp);
    MTS_PY_IMPORT_VARIANTS(qmc);
    MTS_PY_IMPORT_VARIANTS(spline);
    MTS_PY_IMPORT_VARIANTS(quad);
    MTS_PY_IMPORT_VARIANTS(DiscreteDistribution);
    MTS_PY_IMPORT_VARIANTS(AnimatedTransform);
    MTS_PY_IMPORT_VARIANTS(MemoryMappedFile);
    MTS_PY_IMPORT_VARIANTS(ProgressReporter);

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
    m_parent.def(
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
