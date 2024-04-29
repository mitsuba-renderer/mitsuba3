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

#if !defined(_WIN32)
#  include <signal.h>
#else
#  include <windows.h>
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

    -m, --mode
        Request a specific mode/variant of the renderer

        Default: )" MI_DEFAULT_VARIANT R"(

        Available:
              )" << string::indent(MI_VARIANTS, 14) << R"(
    -v, --verbose
        Be more verbose. (can be specified multiple times)

    -t <count>, --threads <count>
        Render with the specified number of threads.

    -D <key>=<value>, --define <key>=<value>
        Define a constant that can referenced as "$key" within the scene
        description.

    -s <index>, --sensor <index>
        Index of the sensor to render with (following the declaration order
        in the scene file). Default value: 0.

    -u, --update
        When specified, Mitsuba will update the scene's XML description
        to the latest version.

    -a <path1>;<path2>;.., --append <path1>;<path2>
        Add one or more entries to the resource search path.

    -o <filename>, --output <filename>
        Write the output image to the file "filename".

 === The following options are only relevant for JIT (CUDA/LLVM) modes ===

    -O [0-5]
        Enables successive optimizations (default: -O5):
          (0. all disabled, 1: de-duplicate virtual functions,
           2: constant propagation, 3. value numbering,
           4. virtual call optimizations, 5. loop optimizations)

    -S
        Dump the PTX or LLVM intermediate representation to the console

    -W
        Instead of compiling a megakernel, perform rendering using a
        series of wavefronts. Specify twice to unroll both loops *and*
        virtual function calls.

    -V <width>
        Override the vector width of the LLVM backend ('width' must be
        a power of two). Values of 4/8/16 cause SSE/NEON, AVX, or AVX512
        registers being used (if supported). Going beyond the natively
        supported width is legal and causes arithmetic operations to be
        replicated multiple times.

)";
}

std::function<void(void)> develop_callback;
std::mutex develop_callback_mutex;

template <typename Float, typename Spectrum>
void scene_static_accel_initialization() {
    Scene<Float, Spectrum>::static_accel_initialization();
}

template <typename Float, typename Spectrum>
void scene_static_accel_shutdown() {
    Scene<Float, Spectrum>::static_accel_shutdown();
}

template <typename Float, typename Spectrum>
void render(Object *scene_, size_t sensor_i, fs::path filename) {
    auto *scene = dynamic_cast<Scene<Float, Spectrum> *>(scene_);
    if (!scene)
        Throw("Root element of the input file must be a <scene> tag!");
    if (scene->sensors().empty())
        Throw("No sensor specified for scene: %s", scene);
    if (sensor_i >= scene->sensors().size())
        Throw("Specified sensor index is out of bounds!");
    auto film = scene->sensors()[sensor_i]->film();

    auto integrator = scene->integrator();
    if (!integrator)
        Throw("No integrator specified for scene: %s", scene);

    /* critical section */ {
        std::lock_guard<std::mutex> guard(develop_callback_mutex);
        develop_callback = [&]() { film->write(filename); };
    }

    integrator->render(scene, (uint32_t) sensor_i,
                       0 /* seed */,
                       0 /* spp */,
                       false /* develop */,
                       true /* evaluate */);

    /* critical section */ {
        std::lock_guard<std::mutex> guard(develop_callback_mutex);
        develop_callback = nullptr;
    }

    film->write(filename);
}

#if !defined(_WIN32)
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
    auto arg_paths     = parser.add(StringVec{ "-a" }, true);
    auto arg_extra     = parser.add("", true);

    // Specialized flags for the JIT compiler
    auto arg_optim_lev = parser.add(StringVec{ "-O" }, true);
    auto arg_wavefront = parser.add(StringVec{ "-W" });
    auto arg_source    = parser.add(StringVec{ "-S" });
    auto arg_vec_width = parser.add(StringVec{ "-V" }, true);

    xml::ParameterList params;
    std::string error_msg, mode;

#if !defined(_WIN32)
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

#if defined(NDEBUG)
        int log_level = 0;
#else
        int log_level = 1;
#endif
        auto arg = arg_verbose;
        while (arg && *arg) {
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

#if defined(MI_ENABLE_CUDA) || defined(MI_ENABLE_LLVM)
        ::LogLevel log_level_drjit[] = {
            ::LogLevel::Error,
            ::LogLevel::Warn,
            ::LogLevel::Info,
            ::LogLevel::InfoSym,
            ::LogLevel::Debug,
            ::LogLevel::Trace
        };
        jit_set_log_level_stderr(log_level_drjit[std::min(log_level, 6)]);
#endif

        // Initialize nanothread with the requested number of threads
        size_t thread_count = Thread::thread_count();
        if (*arg_threads) {
            thread_count = arg_threads->as_int();
            if (thread_count < 1) {
                Log(Warn, "Thread count should be greater than 0. It will be "
                          "set to 1 instead.");
                thread_count = 1;
            }
        }
        Thread::set_thread_count(thread_count);

        while (arg_define && *arg_define) {
            std::string value = arg_define->as_string();
            auto sep = value.find('=');
            if (sep == std::string::npos)
                Throw("-D/--define: expect key=value pair!");
            params.emplace_back(value.substr(0, sep), value.substr(sep+1), false);
            arg_define = arg_define->next();
        }
        mode = (*arg_mode ? arg_mode->as_string() : MI_DEFAULT_VARIANT);
        bool cuda = string::starts_with(mode, "cuda_");
        bool llvm = string::starts_with(mode, "llvm_");

#if defined(MI_ENABLE_CUDA)
        if (cuda)
            jit_init((uint32_t) JitBackend::CUDA);
#endif

#if defined(MI_ENABLE_LLVM)
        if (llvm)
            jit_init((uint32_t) JitBackend::LLVM);
#endif

#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA)
        if (cuda || llvm) {
            if (*arg_optim_lev) {
                int lev = arg_optim_lev->as_int();
                jit_set_flag(JitFlag::VCallDeduplicate, lev > 0);
                jit_set_flag(JitFlag::ConstantPropagation, lev > 1);
                jit_set_flag(JitFlag::ValueNumbering, lev > 2);
                jit_set_flag(JitFlag::VCallOptimize, lev > 3);
                jit_set_flag(JitFlag::LoopOptimize, lev > 4);
            }

            if (*arg_wavefront) {
                jit_set_flag(JitFlag::LoopRecord, false);
                if (arg_wavefront->next())
                    jit_set_flag(JitFlag::VCallRecord, false);
            }

            if (*arg_source)
                jit_set_flag(JitFlag::PrintIR, true);

            if (*arg_vec_width && llvm) {
                uint32_t width = arg_vec_width->as_int();
                if (!math::is_power_of_two(width))
                    Throw("Value specified to the -V argument must be a power of two!");

                std::string target_cpu = jit_llvm_target_cpu(),
                            target_features = jit_llvm_target_features();

                jit_llvm_set_target(target_cpu.c_str(),
                                    target_features.c_str(),
                                    (uint32_t) width);
            }
        }
#else
        DRJIT_MARK_USED(arg_wavefront);
        DRJIT_MARK_USED(arg_optim_lev);
        DRJIT_MARK_USED(arg_source);
#endif

        if (!cuda && !llvm &&
            (*arg_optim_lev || *arg_wavefront || *arg_source || *arg_vec_width))
            Throw("Specified an argument that only makes sense in a JIT (LLVM/CUDA) mode!");

        Profiler::static_initialization();
        color_management_static_initialization(cuda, llvm);

        MI_INVOKE_VARIANT(mode, scene_static_accel_initialization);

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
            help((int) Thread::thread_count());
        } else {
            Log(Info, "%s", util::info_build((int) Thread::thread_count()));
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
            std::vector<ref<Object>> parsed =
                xml::load_file(arg_extra->as_string(), mode, params,
                               *arg_update, false);

            if (parsed.size() != 1)
                Throw("Root element of the input file is expanded into "
                      "multiple objects, only a single object is expected!");

            MI_INVOKE_VARIANT(mode, render, parsed[0].get(), sensor_i, filename);
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

#if defined(_WIN32)
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO console_info;
        GetConsoleScreenBufferInfo(console, &console_info);
        SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_INTENSITY);
#else
        std::cerr << "\x1b[31m";
#endif
        std::cerr << std::endl << error_msg << std::endl;
#if defined(_WIN32)
        SetConsoleTextAttribute(console, console_info.wAttributes);
#else
        std::cerr << "\x1b[0m";
#endif
    }

    MI_INVOKE_VARIANT(mode, scene_static_accel_shutdown);
    color_management_static_shutdown();
    Profiler::static_shutdown();
    Bitmap::static_shutdown();
    StructConverter::static_shutdown();
    Logger::static_shutdown();
    Thread::static_shutdown();
    Class::static_shutdown();
    Jit::static_shutdown();


#if defined(MI_ENABLE_CUDA)
    if (string::starts_with(mode, "cuda_")) {
        printf("%s\n", jit_var_whos());
        jit_shutdown();
    }
#endif

#if defined(MI_ENABLE_LLVM)
    if (string::starts_with(mode, "llvm_")) {
        printf("%s\n", jit_var_whos());
        jit_shutdown();
    }
#endif

    return error_msg.empty() ? 0 : -1;
}
