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

#if !defined(__WINDOWS__)
#  include <signal.h>
#endif

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

    -m, --mode Rendering mode. Defines a combination of floating point
        and color types.

        Default: )" MTS_DEFAULT_VARIANT R"(

        Available modes:
              )" << string::indent(MTS_VARIANTS, 14) << R"(
    -v, --verbose
        Be more verbose. (can be specified multiple times)

    -t <count>, --threads <count>
        Render with the specified number of threads.

    -D <key>=<value>, --define <key>=<value>
        Define a constant that can referenced as "$key"
        within the scene description.

    -s <index>, --sensor <index>
        Index of the sensor to render with (following the declaration
        order in the scene file). Default value: 0.

    -u, --update
        When specified, Mitsuba will update the scene's
        XML description to the latest version.

    -a <path1>;<path2>;..
        Add one or more entries to the resource search path.

    -o <filename>, --output <filename>
        Write the output image to the file "filename".

    -w, --wavefront
        Render in wavefront mode. Can be specified twice to
        disable recording of virtual calls as well.

    -O0
        Disable loop and virtual function call optimizations

    -Ob
        Implement virtual function calls using branching

    -Oo
        Force kernel evaluation in OptiX (esp. in wavefront mode)

    -Os
        Dump the kernel source code to the console

    -g <out>
        Write a GraphViz visualization of the computation
)";
}

std::function<void(void)> develop_callback;
std::mutex develop_callback_mutex;

template <typename Float, typename Spectrum>
bool render(Object *scene_, size_t sensor_i, fs::path filename,
            const fs::path &graphviz_output) {
    auto *scene = dynamic_cast<Scene<Float, Spectrum> *>(scene_);
    if (!scene)
        Throw("Root element of the input file must be a <scene> tag!");
    if (sensor_i >= scene->sensors().size())
        Throw("Specified sensor index is out of bounds!");
    auto sensor = scene->sensors()[sensor_i];
    auto film = sensor->film();

    filename.replace_extension("exr");
    film->set_destination_file(filename);

    auto integrator = scene->integrator();
    if (!integrator)
        Throw("No integrator specified for scene: %s", scene);
    integrator->set_graphviz_output(graphviz_output);

    /* critical section */ {
        std::lock_guard<std::mutex> guard(develop_callback_mutex);
        develop_callback = [&]() { film->develop(); };
    }
    bool success = integrator->render(scene, sensor.get());
    /* critical section */ {
        std::lock_guard<std::mutex> guard(develop_callback_mutex);
        develop_callback = nullptr;
    }
    if (success) {
        film->develop();
    } else {
#if !defined(_WIN32)
        Log(Warn, "\U0000274C Rendering failed, result not saved.");
#else
        Log(Warn, "Rendering failed, result not saved.");
#endif
    }

    return success;
}

#if !defined(__WINDOWS__)
// Handle the hang-up signal and write a partially rendered image to disk
void hup_signal_handler(int signal) {
    if (signal != SIGHUP)
        return;
    std::lock_guard<std::mutex> guard(develop_callback_mutex);
    if (develop_callback)
        develop_callback();
}
#endif

int main(int argc, char *argv[]) {
    Jit::static_initialization();
    Class::static_initialization();
    Thread::static_initialization();
    Logger::static_initialization();
    Bitmap::static_initialization();

    // Ensure that the mitsuba-render shared library is loaded
    librender_nop();

    ArgParser parser;
    using StringVec    = std::vector<std::string>;
    auto arg_threads   = parser.add(StringVec{ "-t", "--threads" }, true);
    auto arg_verbose   = parser.add(StringVec{ "-v", "--verbose" }, false);
    auto arg_define    = parser.add(StringVec{ "-D", "--define" }, true);
    auto arg_sensor_i  = parser.add(StringVec{ "-s", "--sensor" }, true);
    auto arg_output    = parser.add(StringVec{ "-o", "--output" }, true);
    auto arg_update    = parser.add(StringVec{ "-u", "--update" }, false);
    auto arg_help      = parser.add(StringVec{ "-h", "--help" });
    auto arg_mode      = parser.add(StringVec{ "-m", "--mode" }, true);
    auto arg_wavefront = parser.add(StringVec{ "-w", "--wavefront" });
    auto arg_paths     = parser.add(StringVec{ "-a" }, true);
    auto arg_extra     = parser.add("", true);

    auto arg_branch   = parser.add(StringVec{ "-Ob" });
    auto arg_no_optim = parser.add(StringVec{ "-O0" });
    auto arg_force_optix = parser.add(StringVec{ "-Oo" });
    auto arg_dump_source = parser.add(StringVec{ "-Os" });
    auto arg_graphviz  = parser.add(StringVec{ "-g" }, true);

    bool profile = true, print_profile = false;
    xml::ParameterList params;
    std::string error_msg, mode;
    fs::path graphviz_output;

#if !defined(__WINDOWS__)
    /* Initialize signal handlers */
    struct sigaction sa;
    sa.sa_handler = hup_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, nullptr))
        Log(Warn, "Could not install a custom signal handler!");
#endif

    try {
        // Parse all command line options
        parser.parse(argc, argv);

        int log_level = 0;
        auto arg = arg_verbose;
        while (arg) {
            log_level++;
            arg = arg->next();
        }

        // Set the log level
        auto logger = Thread::thread()->logger();
        mitsuba::LogLevel log_level_mitsuba[] = {
            Info,
            Debug,
            Trace
        };

        logger->set_log_level(log_level_mitsuba[std::min(log_level, 2)]);

#if defined(MTS_ENABLE_CUDA) || defined(MTS_ENABLE_LLVM)
        ::LogLevel log_level_enoki[] = {
            ::LogLevel::Error,
            ::LogLevel::Warn,
            ::LogLevel::Info,
            ::LogLevel::InfoSym,
            ::LogLevel::Debug,
            ::LogLevel::Trace
        };
        jit_set_log_level_stderr(log_level_enoki[std::min(log_level, 6)]);
#endif

        // Initialize Intel Thread Building Blocks with the requested number of threads
        size_t thread_count = Thread::thread_count();
        if (*arg_threads)
            thread_count = std::max(1, arg_threads->as_int());
        Thread::set_thread_count(thread_count);

        while (arg_define && *arg_define) {
            std::string value = arg_define->as_string();
            auto sep = value.find('=');
            if (sep == std::string::npos)
                Throw("-D/--define: expect key=value pair!");
            params.push_back(std::make_pair(value.substr(0, sep),
                                            value.substr(sep+1)));
            arg_define = arg_define->next();
        }
        mode = (*arg_mode ? arg_mode->as_string() : MTS_DEFAULT_VARIANT);
        bool cuda = string::starts_with(mode, "cuda_");
        bool llvm = string::starts_with(mode, "llvm_");

#if defined(MTS_ENABLE_CUDA)
        if (cuda) {
            jit_init((uint32_t) JitBackend::CUDA);
            profile = false;
        }
#endif

#if defined(MTS_ENABLE_LLVM)
        if (llvm) {
            jit_init((uint32_t) JitBackend::LLVM);
            profile = false;
        }
#endif

#if defined(MTS_ENABLE_LLVM) || defined(MTS_ENABLE_CUDA)
        if (cuda || llvm) {
            jit_set_flag(JitFlag::VCallBranch, false);

            if (*arg_force_optix)
                jit_set_flag(JitFlag::ForceOptiX, true);

            if (*arg_dump_source)
                jit_set_flag(JitFlag::PrintIR, true);

            if (*arg_no_optim) {
                jit_set_flag(JitFlag::VCallOptimize, false);
                jit_set_flag(JitFlag::LoopOptimize, false);
            }

            if (*arg_branch)
                jit_set_flag(JitFlag::VCallBranch, true);

            if (*arg_wavefront) {
                jit_set_flag(JitFlag::LoopRecord, false);
                if (arg_wavefront->next())
                    jit_set_flag(JitFlag::VCallRecord, false);
            }

            if (*arg_graphviz)
                graphviz_output = arg_graphviz->as_string();
        }
#else
        ENOKI_MARK_USED(arg_force_optix);
        ENOKI_MARK_USED(arg_dump_source);
        ENOKI_MARK_USED(arg_no_optim);
        ENOKI_MARK_USED(arg_branch);
        ENOKI_MARK_USED(arg_wavefront);
        ENOKI_MARK_USED(arg_graphviz);
#endif

        if (profile)
            Profiler::static_initialization();
        cie_static_initialization(cuda, llvm);

        size_t sensor_i  = (*arg_sensor_i ? arg_sensor_i->as_int() : 0);

        // Append the mitsuba directory to the FileResolver search path list
        ref<Thread> thread = Thread::thread();
        ref<FileResolver> fr = thread->file_resolver();
        fs::path base_path = util::library_path().parent_path();
        if (!fr->contains(base_path))
            fr->append(base_path);

        // Append extra paths from command line arguments to the FileResolver search path list
        if (*arg_paths) {
            auto extra_paths = string::tokenize(arg_paths->as_string(), ";");
            for (auto& path : extra_paths) {
                if (!fr->contains(path))
                    fr->append(path);
            }
        }

        if (!*arg_extra || *arg_help) {
            help((int) __global_thread_count);
        } else {
            Log(Info, "%s", util::info_build((int) __global_thread_count));
            Log(Info, "%s", util::info_copyright());
            Log(Info, "%s", util::info_features());

#if !defined(NDEBUG)
            Log(Warn, "Renderer is compiled in debug mode, performance will be considerably reduced.");
#endif
        }

        while (arg_extra && *arg_extra) {
            fs::path filename(arg_extra->as_string());
            ref<FileResolver> fr2 = new FileResolver(*fr);
            thread->set_file_resolver(fr2);

            // Add the scene file's directory to the search path.
            fs::path scene_dir = filename.parent_path();
            if (!fr2->contains(scene_dir))
                fr2->append(scene_dir);

            if (*arg_output)
                filename = arg_output->as_string();

            // Try and parse a scene from the passed file.
            ref<Object> parsed =
                xml::load_file(arg_extra->as_string(), mode, params, *arg_update);

            bool success = MTS_INVOKE_VARIANT(mode, render, parsed.get(),
                                              sensor_i, filename, graphviz_output);
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

    cie_static_shutdown();
    if (profile) {
        Profiler::static_shutdown();
        if (print_profile)
            Profiler::print_report();
    }
    Bitmap::static_shutdown();
    Logger::static_shutdown();
    Thread::static_shutdown();
    Class::static_shutdown();
    Jit::static_shutdown();


#if defined(MTS_ENABLE_CUDA)
    if (string::starts_with(mode, "cuda_")) {
        printf("%s\n", jit_var_whos());
        jit_shutdown();
    }
#endif

#if defined(MTS_ENABLE_LLVM)
    if (string::starts_with(mode, "llvm_")) {
        printf("%s\n", jit_var_whos());
        jit_shutdown();
    }
#endif

    return error_msg.empty() ? 0 : -1;
}
