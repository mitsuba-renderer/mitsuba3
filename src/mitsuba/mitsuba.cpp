#include <mitsuba/core/argparser.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/jit.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/xml.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/render/common.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/scene.h>
#include <tbb/task_scheduler_init.h>

#include <mitsuba/render/records.h>

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

    oss << "Enabled processor features:";
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
    if (enoki::has_x86_64)          oss << " x86_64";
    if (enoki::has_x86_32)          oss << " x86";
    if (enoki::has_neon)            oss << " neon";
    if (enoki::has_arm_32)          oss << " arm";
    if (enoki::has_arm_64)          oss << " aarch64";

#if defined(ENOKI_USE_MEMKIND)
    if (hbw_check_available() == 0) oss << " hbm";
#endif

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

   -D <key>=<value>, --define <key>=<value>
               Define a constant that can referenced as "$key"
               within the scene description

   -s, --scalar
               Render without vectorization support
)";
}

int main(int argc, char *argv[]) {
    Jit::static_initialization();
    Class::static_initialization();
    Thread::static_initialization();
    Logger::static_initialization();
    Bitmap::static_initialization();
    Profiler::static_initialization();

    /* Ensure that the mitsuba-render shared library is loaded */
    librender_nop();

    ArgParser parser;
    using StringVec = std::vector<std::string>;
    auto arg_threads = parser.add(StringVec { "-t", "--threads" }, true);
    auto arg_scalar  = parser.add(StringVec { "-s", "--scalar" }, false);
    auto arg_verbose = parser.add(StringVec { "-v", "--verbose" }, false);
    auto arg_define  = parser.add(StringVec { "-D", "--define" }, true);
    auto arg_help = parser.add(StringVec { "-h", "--help" });
    auto arg_extra = parser.add("", true);
    bool print_profile = false;
    xml::ParameterList params;
    std::string error_msg;

#if defined(__AVX512ER__) && defined(__LINUX__)
    if (getenv("LD_PREFER_MAP_32BIT_EXEC") == nullptr) {
        std::cerr << "Warning: It is strongly recommended that you set the LD_PREFER_MAP_32BIT_EXEC" << std::endl
                  << "environment variable on Xeon Phi machines to avoid misprediction penalties" << std::endl
                  << "involving function calls across 64 bit boundaries, e.g. to Mitsuba plugins." << std::endl
                  << "To do so, enter" << std::endl << std::endl
                  << "   $ export LD_PREFER_MAP_32BIT_EXEC = 1" << std::endl << std::endl
                  << "before launching Mitsuba (you'll want to put this into your .bashrc as well)." << std::endl << std::endl;
    }
#endif

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

        while (arg_define && *arg_define) {
            std::string value = arg_define->as_string();
            auto sep = value.find('=');
            if (sep == std::string::npos)
                Throw("-D/--define: expect key=value pair!");
            params.push_back(std::make_pair(value.substr(0, sep), value.substr(sep+1)));
            arg_define = arg_define->next();
        }

        /* Initialize Intel Thread Building Blocks with the requested number of threads */
        int thread_count = *arg_threads ? arg_threads->as_int() : util::core_count();
        tbb::task_scheduler_init init(thread_count);

        bool render_scalar = (bool) *arg_scalar;

        /* Append the mitsuba directory to the FileResolver search path list */
        ref<FileResolver> fr = Thread::thread()->file_resolver();
        filesystem::path base_path = util::library_path().parent_path();
        if (!fr->contains(base_path))
            fr->append(base_path);

        if (!*arg_extra || *arg_help) {
            help(thread_count);
        } else {
            Log(EInfo, "%s", build_info(thread_count));
            Log(EInfo, "%s", copyright_info());
            Log(EInfo, "%s", isa_info());
            if (render_scalar)
                Log(EInfo, "Vectorization disabled by --scalar flag.");
        }

        while (arg_extra && *arg_extra) {
            filesystem::path filename(arg_extra->as_string());
            // Add the scene file's directory to the search path.
            auto scene_dir = filename.parent_path();
            if (!fr->contains(scene_dir))
                fr->append(scene_dir);

            // Try and parse a scene from the passed file.
            ref<Object> parsed = xml::load_file(arg_extra->as_string(), params);

            auto *scene = dynamic_cast<Scene *>(parsed.get());
            if (scene) {
                filename.replace_extension("exr");
                scene->film()->set_destination_file(filename, 32 /*unused*/);

                auto integrator = scene->integrator();
                if (!integrator)
                    Throw("No integrator specified for scene: %s", scene->to_string());

                bool success = integrator->render(scene, !render_scalar);
                if (success)
                    scene->film()->develop();
                else
                    Log(EWarn, "\U0000274C Rendering failed, result not saved.");
                print_profile = true;
            }

            arg_extra = arg_extra->next();
        }
    } catch (const std::exception &e) {
        error_msg = std::string("Caught a critical exception: ") + e.what();
    } catch (...) {
        error_msg = std::string("Caught a critical exception of unknown type!");
    }

    if (!error_msg.empty()) {
        /* Strip zero-width spaces from the message (Mitsuba uses these
           to properly format chains of multiple exceptions) */
        const std::string zerowidth_space = "\xe2\x80\x8b";
        while (true) {
            auto it = error_msg.find(zerowidth_space);
            if (it == std::string::npos)
                break;
            error_msg = error_msg.substr(0, it) + error_msg.substr(it + 3);
        }

#if defined(__WINDOWS__)
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO console_info;
        GetConsoleScreenBufferInfo(console, &console_info);
        SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_INTENSITY);
#else
        std::cerr << "\x1b[31m";
#endif
        std::cerr << std::endl << error_msg << std::endl;
#if defined(__WINDOWS__)
        SetConsoleTextAttribute(console, console_info.wAttributes);
#else
        std::cerr << "\x1b[0m";
#endif
    }

    Profiler::static_shutdown();
    if (print_profile)
        Profiler::print_report();
    Bitmap::static_shutdown();
    Logger::static_shutdown();
    Thread::static_shutdown();
    Class::static_shutdown();
    Jit::static_shutdown();
    return error_msg.empty() ? 0 : -1;
}
