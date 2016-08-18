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

void help() {
    std::cout << "Mitsuba version " << MTS_VERSION << " (";
#if defined(__WINDOWS__)
	std::cout << "Windows, ";
#elif defined(__LINUX__)
	std::cout << "Linux, ";
#elif defined(__OSX__)
	std::cout << "Mac OS, ";
#else
	std::cout << "Unknown, ";
#endif
    std::cout << (sizeof(size_t) * 8) << "bit), Copyright " << MTS_YEAR
              << " by Wenzel Jakob" << std::endl;

    std::cout << "Instruction sets enabled:";
    if (simd::hasAVX512DQ) std::cout << " avx512dq";
    if (simd::hasAVX512BW) std::cout << " avx512bw";
    if (simd::hasAVX512VL) std::cout << " avx512vl";
    if (simd::hasAVX512ER) std::cout << " avx512eri";
    if (simd::hasAVX512PF) std::cout << " avx512pfi";
    if (simd::hasAVX512CD) std::cout << " avx512cdi";
    if (simd::hasAVX512F)  std::cout << " avx512f";
    if (simd::hasAVX2)     std::cout << " avx2";
    if (simd::hasAVX)      std::cout << " avx";
    if (simd::hasFMA)      std::cout << " fma";
    if (simd::hasF16C)     std::cout << " f16c";
    if (simd::hasSSE42)    std::cout << " sse4.2";
    std::cout << std::endl;

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

#if defined(__GNUC__)
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
        tbb::task_scheduler_init init(
            *arg_threads ? arg_threads->asInt()
                         : tbb::task_scheduler_init::automatic);

        /* Append the mitsuba directory to the FileResolver search path list */
        ref<FileResolver> fr = Thread::thread()->fileResolver();
        fs::path basePath = util::libraryPath().parent_path();
        if (!fr->contains(basePath))
            fr->append(basePath);

        if (!arg_extra || *arg_help)
            help();

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
