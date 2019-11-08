#include <mitsuba/core/argparser.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/jit.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/xml.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <tbb/task_scheduler_init.h>

using namespace mitsuba;

static void help(int thread_count) {
    std::cout << util::info_build(thread_count) << std::endl;
    std::cout << util::info_copyright() << std::endl;
    std::cout << util::info_features() << std::endl;
    std::cout << R"(
Usage: mitsuba [options] <One or more scene XML files>

Options:

    -h, --help
        Display this help text.

    -m, --mode
        Rendering mode. Defines a combination of floating point
        and color types.
        Default mode:
            )" MTS_DEFAULT_MODE R"(
        Available modes:
)"
MTS_CONFIGURATIONS_INDENTED
R"(
    -v, --verbose
        Be more verbose. (can be specified multiple times)

    -t <count>, --threads <count>
        Render with the specified number of threads.

    -D <key>=<value>, --define <key>=<value>
        Define a constant that can referenced as "$key"
        within the scene description.

    -u, --update
        When specified, Mitsuba will update the scene's
        XML description to the latest version.

    -o <filename>, --output <filename>
        Write the output image to the file "filename".
)";
}

template <typename Float, typename Spectrum>
bool render(Object *scene_, filesystem::path filename) {
    auto *scene = dynamic_cast<Scene<Float, Spectrum> *>(scene_);
    if (!scene)
        Throw("Could not cast to Scene<Float, Spectrum>: %s", scene_->to_string());

    filename.replace_extension("exr");
    scene->film()->set_destination_file(filename, 32 /*unused*/);

    auto integrator = scene->integrator();
    if (!integrator)
        Throw("No integrator specified for scene: %s", scene->to_string());

    bool success = integrator->render(scene);
    if (success)
        scene->film()->develop();
    else
        Log(Warn, "\U0000274C Rendering failed, result not saved.");
    return success;
}

template <typename Float, typename Spectrum>
void log_special_modes() {
    Log(Info, "%s", util::info_build(__global_thread_count));
    Log(Info, "%s", util::info_copyright());
    Log(Info, "%s", util::info_features());

    if constexpr (is_monochrome_v<Spectrum>)
        Log(Info, "\U0001F39E Monochrome mode enabled.");

    if constexpr (is_double_v<scalar_t<Float>>)
        Log(Warn, "Double precision mode enabled.");

#if !defined(NDEBUG)
    Log(Warn, "Renderer is compiled in debug mode, performance will be considerably reduced.");
#endif
}

int main(int argc, char *argv[]) {
    Jit::static_initialization();
    Class::static_initialization();
    Thread::static_initialization();
    Logger::static_initialization();
    Bitmap::static_initialization();
    Profiler::static_initialization();

    // Ensure that the mitsuba-render shared library is loaded
    // TODO
    // librender_nop();

    ArgParser parser;
    using StringVec    = std::vector<std::string>;
    auto arg_threads   = parser.add(StringVec{ "-t", "--threads" }, true);
    auto arg_verbose   = parser.add(StringVec{ "-v", "--verbose" }, false);
    auto arg_define    = parser.add(StringVec{ "-D", "--define" }, true);
    auto arg_output    = parser.add(StringVec{ "-o", "--output" }, true);
    auto arg_update    = parser.add(StringVec{ "-u", "--update" }, false);
    auto arg_help      = parser.add(StringVec{ "-h", "--help" });
    auto arg_mode      = parser.add(StringVec{ "-m", "--mode" }, true);
    auto arg_extra     = parser.add("", true);
    bool print_profile = false;
    xml::ParameterList params;
    std::string error_msg;

    try {
        // Parse all command line options
        parser.parse(argc, argv);

        if (*arg_verbose) {
            auto logger = Thread::thread()->logger();
            if (arg_verbose->next())
                logger->set_log_level(Trace);
            else
                logger->set_log_level(Debug);
        }

        while (arg_define && *arg_define) {
            std::string value = arg_define->as_string();
            auto sep = value.find('=');
            if (sep == std::string::npos)
                Throw("-D/--define: expect key=value pair!");
            params.push_back(std::make_pair(value.substr(0, sep),
                                            value.substr(sep+1)));
            arg_define = arg_define->next();
        }
        std::string mode;
        if (*arg_mode)
            mode = arg_mode->as_string();
        else
            mode = MTS_DEFAULT_MODE;

        // Initialize Intel Thread Building Blocks with the requested number of threads
        if (*arg_threads)
            __global_thread_count = arg_threads->as_int();
        if (__global_thread_count < 1)
            Throw("Thread count must be >= 1!");
        tbb::task_scheduler_init init((int) __global_thread_count);

        // Append the mitsuba directory to the FileResolver search path list
        ref<Thread> thread = Thread::thread();
        ref<FileResolver> fr = thread->file_resolver();
        filesystem::path base_path = util::library_path().parent_path();
        if (!fr->contains(base_path))
            fr->append(base_path);

        if (!*arg_extra || *arg_help) {
            help(__global_thread_count);
        } else {
            MTS_ROUTE_MODE(mode, log_special_modes);
        }

        while (arg_extra && *arg_extra) {
            filesystem::path filename(arg_extra->as_string());
            ref<FileResolver> fr2 = new FileResolver(*fr);
            thread->set_file_resolver(fr2);

            // Add the scene file's directory to the search path.
            fs::path scene_dir = filename.parent_path();
            if (!fr2->contains(scene_dir))
                fr2->append(scene_dir);

            if (*arg_output) {
                filesystem::path output(arg_output->as_string());
                filename = filename.parent_path() / output;
            }

            // Try and parse a scene from the passed file.
            ref<Object> parsed =
                xml::load_file(arg_extra->as_string(), mode, params, *arg_update);

            bool success = MTS_ROUTE_MODE(mode, render, parsed.get(), filename);
            print_profile = print_profile || success;
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
