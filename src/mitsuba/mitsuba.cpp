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

int main(int argc, char *argv[]) {
    Jit::staticInitialization();
    Class::staticInitialization();
    Thread::staticInitialization();
    Logger::staticInitialization();

    std::cout << "Mitsuba 2 (flags:";
    if (simd::has_avx512dq)
        std::cout << " avx512dq";
    if (simd::has_avx512vl)
        std::cout << " avx512vl";
    if (simd::has_avx512f)
        std::cout << " avx512f";
    if (simd::has_avx2)
        std::cout << " avx2";
    if (simd::has_fma)
        std::cout << " fma";
    if (simd::has_f16c)
        std::cout << " f16c";
    if (simd::has_avx)
        std::cout << " avx";
    if (simd::has_sse4_2)
        std::cout << " sse4.2";
    std::cout << ")" << std::endl;

    ArgParser parser;
    typedef std::vector<std::string> StringVec;
    auto arg_threads = parser.add(StringVec { "-t", "--threads" }, true);
    auto arg_extra = parser.add("", true);

    try {
        /* Parse all command line options */
        parser.parse(argc, argv);

        /* Initialize Intel Thread Building Blocks with the requested number of threads */
        tbb::task_scheduler_init init(
            *arg_threads ? arg_threads->asInt()
                         : tbb::task_scheduler_init::automatic);

        /* Append the mitsuba directory to the FileResolver search path list */
        ref<FileResolver> fr = Thread::thread()->fileResolver();
        fs::path basePath = util::libraryPath().parent_path();
        if (!fr->contains(basePath))
            fr->append(basePath);

        while (arg_extra && *arg_extra) {
            xml::loadFile(arg_extra->asString());
            arg_extra = arg_extra->next();
        }
	} catch (const std::exception &e) {
		std::cerr << "Caught a critical exception: " << e.what() << std::endl;
		return -1;
	} catch (...) {
		std::cerr << "Caught a critical exception of unknown type!" << std::endl;
		return -1;
	}

    Logger::staticShutdown();
    Thread::staticShutdown();
    Class::staticShutdown();
    Jit::staticShutdown();
    return 0;
}
