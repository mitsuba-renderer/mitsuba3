#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/xml.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/argparser.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/jit.h>
#include <tbb/task_scheduler_init.h>

#include <mitsuba/render/shape.h>

using namespace mitsuba;

static std::string buildInfo(int threadCount) {
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
    oss << threadCount << " thread" << (threadCount > 1 ? "s" : "");
    oss << ")";

    return oss.str();
}

static std::string copyrightInfo() {
    std::ostringstream oss;
    oss << "Copyright " << MTS_YEAR << " by " << MTS_AUTHORS;
    return oss.str();
}

static std::string isaInfo() {
    std::ostringstream oss;

    oss << "Instruction sets enabled:";
    if (simd::hasAVX512DQ) oss << " avx512dq";
    if (simd::hasAVX512BW) oss << " avx512bw";
    if (simd::hasAVX512VL) oss << " avx512vl";
    if (simd::hasAVX512ER) oss << " avx512eri";
    if (simd::hasAVX512PF) oss << " avx512pfi";
    if (simd::hasAVX512CD) oss << " avx512cdi";
    if (simd::hasAVX512F)  oss << " avx512f";
    if (simd::hasAVX2)     oss << " avx2";
    if (simd::hasAVX)      oss << " avx";
    if (simd::hasFMA)      oss << " fma";
    if (simd::hasF16C)     oss << " f16c";
    if (simd::hasSSE42)    oss << " sse4.2";

    return oss.str();
}

static void help(int threadCount) {
    std::cout << buildInfo(threadCount) << std::endl;
    std::cout << copyrightInfo() << std::endl;
    std::cout << isaInfo() << std::endl;
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
    // target<->host mismatches can be caught in Jit::staticInitialization()
    // without running into a segmentation fault before that.
    __attribute__((target("sse")))
#endif

int main(int argc, char *argv[]) {
    Jit::staticInitialization();
    Class::staticInitialization();
    Thread::staticInitialization();
    Logger::staticInitialization();

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
                logger->setLogLevel(ETrace);
            else
                logger->setLogLevel(EDebug);
        }

        /* Initialize Intel Thread Building Blocks with the requested number of threads */
        int threadCount = *arg_threads ? arg_threads->asInt() : util::coreCount();
        tbb::task_scheduler_init init(threadCount);

        /* Append the mitsuba directory to the FileResolver search path list */
        ref<FileResolver> fr = Thread::thread()->fileResolver();
        fs::path basePath = util::libraryPath().parent_path();
        if (!fr->contains(basePath))
            fr->append(basePath);

        if (!*arg_extra || *arg_help) {
            help(threadCount);
        } else {
            Log(EInfo, "%s", buildInfo(threadCount));
            Log(EInfo, "%s", copyrightInfo());
            Log(EInfo, "%s", isaInfo());
        }

        while (arg_extra && *arg_extra) {
            xml::loadFile(arg_extra->asString());
            arg_extra = arg_extra->next();
        }
    } catch (const std::exception &e) {
        std::cerr << "\nCaught a critical exception: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "\nCaught a critical exception of unknown type!" << std::endl;
        return -1;
    }

    Logger::staticShutdown();
    Thread::staticShutdown();
    Class::staticShutdown();
    Jit::staticShutdown();
    return 0;
}
