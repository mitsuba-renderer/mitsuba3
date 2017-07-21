#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/xml.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/argparser.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/jit.h>
#include <mitsuba/core/bitmap.h>
#include <tbb/task_scheduler_init.h>

#include <mitsuba/render/shape.h>

using namespace mitsuba;

static std::string build_info(int thread_count) {
    std::ostringstream oss;
    oss << "Mitsuba version " << MTS_VERSION << " (";
    oss << MTS_BRANCH << "[" << MTS_HASH << "], ";
#if defined(__WINDOWS__)
    oss << "Windows, ";
#elif defined(__LINUX__)
    oss << "Linux, ";
#elif defined(__OSX__)
    oss << "Mac OS, ";
#else
    oss << "Unknown, ";
#endif
    oss << (sizeof(size_t) * 8) << "bit, ";
    oss << thread_count << " thread" << (thread_count > 1 ? "s" : "");
    oss << ")";

    return oss.str();
}

static std::string copyright_info() {
    std::ostringstream oss;
    oss << "Copyright " << MTS_YEAR << " by " << MTS_AUTHORS;
    return oss.str();
}

static std::string isa_info() {
    std::ostringstream oss;

    oss << "Instruction sets enabled:";
    if (enoki::has_avx512f)         oss << " avx512f";
    if (enoki::has_avx512cd)        oss << " avx512cd";
    if (enoki::has_avx512dq)        oss << " avx512dq";
    if (enoki::has_avx512vl)        oss << " avx512vl";
    if (enoki::has_avx512bw)        oss << " avx512bw";
    if (enoki::has_avx512pf)        oss << " avx512pf";
    if (enoki::has_avx512er)        oss << " avx512er";
    if (enoki::has_avx512vpopcntdq) oss << " avx512vpopcntdq";
    if (enoki::has_avx2)            oss << " avx2";
    if (enoki::has_avx)             oss << " avx";
    if (enoki::has_fma)             oss << " fma";
    if (enoki::has_f16c)            oss << " f16c";
    if (enoki::has_sse42)           oss << " sse4.2";

    return oss.str();
}

static void help(int thread_count) {
    std::cout << build_info(thread_count) << std::endl;
    std::cout << copyright_info() << std::endl;
    std::cout << isa_info() << std::endl;
    std::cout << R"(
Usage: mitsuba [options] <One or more scene XML files>

Options:

   -h, --help
               Display this help text

   -v, --verbose
               Be more verbose (can be specified multiple times)

   -t <count>, --threads <count>
               Render with the specified number of threads
)";
}

#if defined(__GNUC__) && !defined(__INTEL_COMPILER)
    // Limit instruction set usage in main() to SSE only. This is so that
    // target<->host mismatches can be caught in Jit::static_initialization()
    // without running into a segmentation fault before that.
    __attribute__((target("sse")))
#endif

int main(int argc, char *argv[]) {
    Jit::static_initialization();
    Class::static_initialization();
    Thread::static_initialization();
    Logger::static_initialization();
    Bitmap::static_initialization();

    ArgParser parser;
    typedef std::vector<std::string> StringVec;
    auto arg_threads = parser.add(StringVec { "-t", "--threads" }, true);
    auto arg_verbose = parser.add(StringVec { "-v", "--verbose" }, false);
    auto arg_help = parser.add(StringVec { "-h", "--help" });
    auto arg_extra = parser.add("", true);

    try {
        /* Parse all command line options */
        parser.parse(argc, argv);

        if (*arg_verbose) {
            auto logger = Thread::thread()->logger();
            if (arg_verbose->next())
                logger->set_log_level(ETrace);
            else
                logger->set_log_level(EDebug);
        }

        /* Initialize Intel Thread Building Blocks with the requested number of threads */
        int thread_count = *arg_threads ? arg_threads->as_int() : util::core_count();
        tbb::task_scheduler_init init(thread_count);

        /* Append the mitsuba directory to the FileResolver search path list */
        ref<FileResolver> fr = Thread::thread()->file_resolver();
        fs::path base_path = util::library_path().parent_path();
        if (!fr->contains(base_path))
            fr->append(base_path);

        if (!*arg_extra || *arg_help) {
            help(thread_count);
        } else {
            Log(EInfo, "%s", build_info(thread_count));
            Log(EInfo, "%s", copyright_info());
            Log(EInfo, "%s", isa_info());
        }

        while (arg_extra && *arg_extra) {
            xml::load_file(arg_extra->as_string());
            arg_extra = arg_extra->next();
        }
    } catch (const std::exception &e) {
        std::cerr << "\nCaught a critical exception: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "\nCaught a critical exception of unknown type!" << std::endl;
        return -1;
    }

    Bitmap::static_shutdown();
    Logger::static_shutdown();
    Thread::static_shutdown();
    Class::static_shutdown();
    Jit::static_shutdown();
    return 0;
}
