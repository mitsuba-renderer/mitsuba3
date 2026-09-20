#include <mitsuba/core/argparser.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/parser.h>
#include <nanothread/nanothread.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/srgb.h>
#include <functional>

#if !defined(_WIN32)
#  include <signal.h>
#else
#  include <windows.h>
#endif

using namespace mitsuba;

/// Initialize the JIT backend a variant requires; return whether it is
/// available. Scalar variants need no backend and always succeed.
static bool init_variant_backend(std::string_view variant) {
    if (string::starts_with(variant, "scalar_"))
        return true;

#if defined(MI_ENABLE_CUDA)
    if (string::starts_with(variant, "cuda_")) {
        jit_init(1u << (uint32_t) JitBackend::CUDA);
        return jit_has_backend(JitBackend::CUDA);
    }
#endif

#if defined(MI_ENABLE_LLVM)
    if (string::starts_with(variant, "llvm_")) {
        jit_init(1u << (uint32_t) JitBackend::LLVM);
        return jit_has_backend(JitBackend::LLVM);
    }
#endif

#if defined(MI_ENABLE_METAL)
    if (string::starts_with(variant, "metal_")) {
        jit_init(1u << (uint32_t) JitBackend::Metal);
        return jit_has_backend(JitBackend::Metal);
    }
#endif

    return false;
}

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

        Default: the most capable variant whose backend is available at
        runtime (preferring an RGB color representation).

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

    -a <path1>;<path2>;.., --append <path1>;<path2>
        Add one or more entries to the resource search path.

    -o <filename>, --output <filename>
        Write the output image to the file "filename".

 === The following options are only relevant for JIT (CUDA/LLVM) modes ===

    -S
        Dump the PTX or LLVM intermediate representation to the console

)";
}

static std::function<void()> develop_callback_fn = nullptr;
static void develop_callback() {
    if (develop_callback_fn)
        develop_callback_fn();
}

template <typename Float, typename Spectrum>
void scene_static_accel_initialization() {
    Scene<Float, Spectrum>::static_accel_initialization();
}

template <typename Float, typename Spectrum>
void scene_static_accel_shutdown() {
    Scene<Float, Spectrum>::static_accel_shutdown();
}

template <typename Float, typename Spectrum>
void srgb_model_static_shutdown() {
    SRGBModel<Float, Spectrum>::static_shutdown();
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

    develop_callback_fn = [film]() { film->develop(); };

    integrator->render(scene, (uint32_t) sensor_i,
                       0 /* seed */,
                       0 /* spp */,
                       false /* develop */,
                       true /* evaluate */,
                       true /* profile */);

    develop_callback_fn = nullptr;

    film->write(filename);
}

#if !defined(_WIN32)
// Handle the hang-up signal and write a partially rendered image to disk
void hup_signal_handler(int signal) {
    if (signal != SIGHUP)
        return;
    develop_callback();
}
#endif

int main(int argc, char *argv[]) {
    Thread::static_initialization();
    Logger::static_initialization();
    Bitmap::static_initialization();

    ArgParser parser;
    using StringVec    = std::vector<std::string>;
    auto arg_threads   = parser.add(StringVec{ "-t", "--threads" }, true);
    auto arg_verbose   = parser.add(StringVec{ "-v", "--verbose" }, false);
    auto arg_define    = parser.add(StringVec{ "-D", "--define" }, true);
    auto arg_sensor_i  = parser.add(StringVec{ "-s", "--sensor" }, true);
    auto arg_output    = parser.add(StringVec{ "-o", "--output" }, true);
    auto arg_help      = parser.add(StringVec{ "-h", "--help" });
    auto arg_mode      = parser.add(StringVec{ "-m", "--mode" }, true);
    auto arg_paths     = parser.add(StringVec{ "-a" }, true);
    auto arg_extra     = parser.add("", true);

    // Specialized flags for the JIT compiler
    auto arg_source    = parser.add(StringVec{ "-S" });

    parser::ParameterList params;
    std::string error_msg, mode;

#if !defined(_WIN32)
    // Initialize signal handlers
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

#if defined(MI_ENABLE_CUDA) || defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_METAL)
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
        uint32_t thread_count = pool_size();
        if (*arg_threads) {
            thread_count = arg_threads->as_int();
            if (thread_count < 1) {
                Log(Warn, "Thread count should be greater than 0. It will be "
                          "set to 1 instead.");
                thread_count = 1;
            }
        }
        pool_set_size(nullptr, thread_count);

        while (arg_define && *arg_define) {
            std::string value = arg_define->as_string();
            auto sep = value.find('=');
            if (sep == std::string::npos)
                Throw("-D/--define: expect key=value pair!");
            params.emplace_back(value.substr(0, sep), value.substr(sep+1));
            arg_define = arg_define->next();
        }
        if (*arg_mode) {
            mode = arg_mode->as_string();
            init_variant_backend(mode);
        } else if (*arg_extra && !*arg_help) {
            // Pick the most capable variant whose backend is available
            for (const std::string &v : string::tokenize(MI_VARIANTS, "\n")) {
                if (init_variant_backend(v)) {
                    mode = v;
                    break;
                }
            }
        } else {
            mode = "scalar_rgb";
        }

        bool cuda  = string::starts_with(mode, "cuda_");
        bool llvm  = string::starts_with(mode, "llvm_");
        bool metal = string::starts_with(mode, "metal_");
        bool jit   = cuda || llvm || metal;

#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA) || defined(MI_ENABLE_METAL)
        if (jit && *arg_source)
            jit_set_flag(JitFlag::PrintIR, true);
#else
        DRJIT_MARK_USED(arg_source);
#endif

        if (!jit && *arg_source)
            Throw("Specified an argument that only makes sense in a JIT (LLVM/CUDA/Metal) mode!");

        Profiler::static_initialization();
        color_management_static_initialization(cuda, llvm, metal);

        MI_INVOKE_VARIANT(mode, scene_static_accel_initialization);

        size_t sensor_i  = (*arg_sensor_i ? arg_sensor_i->as_int() : 0);

        // Append the mitsuba directory to the FileResolver search path list
        ref<Thread> thread = Thread::thread();
        ref<FileResolver> fr = file_resolver();
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
            help(pool_size());
        } else {
            Log(Info, "%s", util::info_build(pool_size()));
            Log(Info, "%s", util::info_copyright());
            Log(Info, "%s", util::info_features());
            Log(Info, "Rendering using the \"%s\" variant.", mode);

#if !defined(NDEBUG)
            Log(Warn, "Renderer is compiled in debug mode, performance will be considerably reduced.");
#endif
        }

        parser::ParserConfig config(mode);

        while (arg_extra && *arg_extra) {
            fs::path filename(arg_extra->as_string());
            if (*arg_output)
                filename = fs::path(arg_output->as_string());

            // Parse the XML file
            parser::ParserState state = parser::parse_file(
                config, arg_extra->as_string(), params);

            // Resolve references an optimize the scene representation
            parser::transform_all(config, state);

            // Instantiate scene objects in parallel
            std::vector<ref<Object>> objects =
                parser::instantiate(config, state);

            if (objects.size() != 1)
                Throw("Root element of the input file is expanded into "
                      "multiple objects, only a single object is expected!");

            MI_INVOKE_VARIANT(mode, render, objects[0].get(), sensor_i, filename);
            arg_extra = arg_extra->next();
        }
    } catch (const std::exception &e) {
        error_msg = std::string("Caught a critical exception: ") + e.what();
    } catch (...) {
        error_msg = std::string("Caught a critical exception of unknown type!");
    }

    if (!error_msg.empty()) {
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
    MI_INVOKE_VARIANT(mode, srgb_model_static_shutdown);
    color_management_static_shutdown();
    Profiler::static_shutdown();
    Bitmap::static_shutdown();
    struct_jit::clear_cache();
    Logger::static_shutdown();
    Thread::static_shutdown();


#if defined(MI_ENABLE_CUDA)
    if (string::starts_with(mode, "cuda_"))
        jit_shutdown();
#endif

#if defined(MI_ENABLE_LLVM)
    if (string::starts_with(mode, "llvm_"))
        jit_shutdown();
#endif

#if defined(MI_ENABLE_METAL)
    if (string::starts_with(mode, "metal_"))
        jit_shutdown();
#endif

    return error_msg.empty() ? 0 : -1;
}
