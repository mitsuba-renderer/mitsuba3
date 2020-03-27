#include <tbb/tbb.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/jit.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/python/python.h>

MTS_PY_DECLARE(atomic);
MTS_PY_DECLARE(filesystem);
MTS_PY_DECLARE(quad);
MTS_PY_DECLARE(Object);
MTS_PY_DECLARE(Cast);
MTS_PY_DECLARE(Struct);
MTS_PY_DECLARE(Appender);
MTS_PY_DECLARE(ArgParser);
MTS_PY_DECLARE(Bitmap);
MTS_PY_DECLARE(Formatter);
MTS_PY_DECLARE(FileResolver);
MTS_PY_DECLARE(Logger);
MTS_PY_DECLARE(MemoryMappedFile);
MTS_PY_DECLARE(Stream);
MTS_PY_DECLARE(DummyStream);
MTS_PY_DECLARE(FileStream);
MTS_PY_DECLARE(MemoryStream);
MTS_PY_DECLARE(ZStream);
MTS_PY_DECLARE(ProgressReporter);
MTS_PY_DECLARE(rfilter);
MTS_PY_DECLARE(Thread);
MTS_PY_DECLARE(util);

PYBIND11_MODULE(core_ext, m) {
    // Temporarily change the module name (for pydoc)
    m.attr("__name__") = "mitsuba.core";

    // Expose some constants in the main `mitsuba` module
    m.attr("__version__")       = MTS_VERSION;
    m.attr("MTS_VERSION")       = MTS_VERSION;
    m.attr("MTS_VERSION_MAJOR") = MTS_VERSION_MAJOR;
    m.attr("MTS_VERSION_MINOR") = MTS_VERSION_MINOR;
    m.attr("MTS_VERSION_PATCH") = MTS_VERSION_PATCH;
    m.attr("MTS_YEAR")          = MTS_YEAR;
    m.attr("MTS_AUTHORS")       = MTS_AUTHORS;

#if defined(NDEBUG)
    m.attr("DEBUG") = false;
#else
    m.attr("DEBUG") = true;
#endif

#if defined(MTS_ENABLE_OPTIX)
    m.attr("MTS_ENABLE_OPTIX") = true;
#else
    m.attr("MTS_ENABLE_OPTIX") = false;
#endif

#if defined(MTS_ENABLE_EMBREE)
    m.attr("MTS_ENABLE_EMBREE") = true;
#else
    m.attr("MTS_ENABLE_EMBREE") = false;
#endif

    Jit::static_initialization();
    Class::static_initialization();
    Thread::static_initialization();
    Logger::static_initialization();
    Bitmap::static_initialization();

    // Append the mitsuba directory to the FileResolver search path list
    ref<FileResolver> fr = Thread::thread()->file_resolver();
    fs::path base_path = util::library_path().parent_path();
    if (!fr->contains(base_path))
        fr->append(base_path);

    /* Holds a pointer to the current TBB task scheduler to enable
       later changes via set_thread_count(). Cleaned up in
       \ref cleanup_callback. */
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
        "Sets the maximum number of threads to be used for parallelized "
        "execution of Mitsuba code. Defaults to -1 (automatic).");

    py::module enoki_scalar = py::module::import("enoki.scalar");
    pybind11_type_alias<Array<uint32_t, 2>, Vector<uint32_t, 2>>();
    pybind11_type_alias<Array<uint32_t, 2>, Point<uint32_t, 2>>();
    pybind11_type_alias<Array<int32_t, 2>, Vector<int32_t, 2>>();
    pybind11_type_alias<Array<int32_t, 2>, Point<int32_t, 2>>();
    pybind11_type_alias<Array<float, 3>, Vector<float, 3>>();
    pybind11_type_alias<Array<float, 3>, Point<float, 3>>();

    // Register python modules
    MTS_PY_IMPORT(atomic);
    MTS_PY_IMPORT(filesystem);
    MTS_PY_IMPORT(quad);
    MTS_PY_IMPORT(Object);
    MTS_PY_IMPORT(Cast);
    MTS_PY_IMPORT(Struct);
    MTS_PY_IMPORT(Appender);
    MTS_PY_IMPORT(ArgParser);
    MTS_PY_IMPORT(rfilter);
    MTS_PY_IMPORT(Stream);
    MTS_PY_IMPORT(Bitmap);
    MTS_PY_IMPORT(Formatter);
    MTS_PY_IMPORT(FileResolver);
    MTS_PY_IMPORT(Logger);
    MTS_PY_IMPORT(MemoryMappedFile);
    MTS_PY_IMPORT(DummyStream);
    MTS_PY_IMPORT(FileStream);
    MTS_PY_IMPORT(MemoryStream);
    MTS_PY_IMPORT(ZStream);
    MTS_PY_IMPORT(ProgressReporter);
    MTS_PY_IMPORT(Thread);
    MTS_PY_IMPORT(util);

    /* Register a cleanup callback function that is invoked when
       the 'mitsuba::Object' Python type is garbage collected */
    py::cpp_function cleanup_callback(
        [scheduler_holder](py::handle weakref) {
            delete scheduler_holder;

            Bitmap::static_shutdown();
            Logger::static_shutdown();
            Thread::static_shutdown();
            Class::static_shutdown();
            Jit::static_shutdown();
            weakref.dec_ref();
        }
    );

    (void) py::weakref(m.attr("Object"), cleanup_callback).release();

    // Change module name back to correct value
    m.attr("__name__") = "mitsuba.core_ext";
}
