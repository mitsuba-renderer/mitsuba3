# ----------------------------------------------------------
# Top-level Mitsuba 2 CMake file, requires a recent version.
# ----------------------------------------------------------
cmake_minimum_required (VERSION 3.13...3.18)

project(mitsuba
  DESCRIPTION
    "Mitsuba"
  LANGUAGES
    CXX C
)

# ----------------------------------------------------------
#  Optional features available to users
# ----------------------------------------------------------

# A number of Mitsuba 2 features (scripting, differentiable rendering, testing
# infrastructure, etc.) are only available if Python bindings are built.
option(MTS_ENABLE_PYTHON "Build Python bindings for Mitsuba, Enoki, and NanoGUI?" ON)

# Mitsuba 2 ships with its own ray tracing acceleration data structure. If
# desired, Embree (which tends to be quite a bit faster) can be used instead.
# Embree lacks some features provided by Mitsuba 2, like support for double
# precision arithmetic.
option(MTS_ENABLE_EMBREE  "Use Embree for ray tracing operations?" ON)

# Build the graphical user interface? (Note that this feature remains incomplete)
option(MTS_ENABLE_GUI     "Build the graphical user interface?" OFF)

# if (UNIX)
#   # Mitsuba 2 ships with a built-in sampling profiler that highlights where
#   # rendering time is spent. Only supported on Linux/macOS at the moment.
#   option(MTS_ENABLE_PROFILER "Enable sampling profiler" OFF)
# endif()

# Use GCC/Clang address sanitizer?
# NOTE: To use this in conjunction with Python plugin, you will need to call
# On OSX:
#   export DYLD_INSERT_LIBRARIES=<path to libclang_rt.asan_osx_dynamic.dylib>
# On Linux:
#   export LD_LIBRARY_PATH=<path to libasan.so>

option(MTS_SANITIZE_ADDRESS "Enable GCC/Clang address sanitizer?" OFF) # To catch out-of-bounds accesses
option(MTS_SANITIZE_MEMORY  "Enable GCC/Clang memory sanitizer?"  OFF) # To catch use of unitialized memory

option(MTS_THROW_TRAPS_DEBUGGER "Trap the debugger on calls to `Throw`?" OFF)
if(MTS_THROW_TRAPS_DEBUGGER)
  add_definitions(-DMTS_THROW_TRAPS_DEBUGGER)
endif()

option(MTS_PROFILER_ITTNOTIFY "Forward profiler events (to Intel VTune)?" OFF)
option(MTS_PROFILER_NVTX      "Forward profiler events (to NVIDIA Nsight)?" OFF)

# ----------------------------------------------------------
#  Check if submodules have been checked out, or fail early
# ----------------------------------------------------------

if (NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ext/enoki/ext/enoki-jit/ext/enoki-thread/ext/cmake-defaults")
  message(FATAL_ERROR "The Mitsuba 2 dependencies are missing! "
    "You probably did not clone the project with --recursive. It is possible to recover "
    "by invoking\n$ git submodule update --init --recursive")
endif()

# ----------------------------------------------------------
#  In-tree builds are not permitted
# ----------------------------------------------------------

if (PROJECT_SOURCE_DIR STREQUAL "${PROJECT_BINARY_DIR}")
  message(FATAL_ERROR "In-tree builds are not permitted. To recover, delete "
          "'CMakeCache.txt', the 'CMakeFiles' directory and inform CMake about "
          "the source (-S) and build (-B) paths. For example to compile to a "
          "directory labeled 'build' using the Ninja generator, enter\n"
          "  $ rm -Rf CMakeCache CMakeFiles\n"
          "  $ cmake -S . -B build -G Ninja"
          "  $ cmake --build build")
endif()

# ----------------------------------------------------------
#  Build defaults for projects by the Realistic Graphics Lab
# ----------------------------------------------------------

set(P "MTS") # <-- prefix for CMake variables, used by the following script
set(MTS_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/include/mitsuba/mitsuba.h")
include(ext/enoki/ext/enoki-jit/ext/enoki-thread/ext/cmake-defaults/CMakeLists.txt)

# ----------------------------------------------------------
#  scikit-build: find installed version of dependencies
# ----------------------------------------------------------

if (SKBUILD)
  execute_process(
    COMMAND
    "${PYTHON_EXECUTABLE}" -c
    "import enoki; print(enoki.get_cmake_dir())"
    OUTPUT_VARIABLE _tmp_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ECHO STDOUT)
  list(APPEND CMAKE_PREFIX_PATH "${_tmp_dir}")
  find_package(enoki CONFIG REQUIRED)
endif()

# ----------------------------------------------------------
#  Create the 'mitsuba.conf' file if none is found
# ----------------------------------------------------------

if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/mitsuba.conf)
  # Default: only scalar modes (RGB, Spectral)
  set(MTS_DEFAULT_VARIANTS "scalar_rgb;scalar_spectral;cuda_ad_rgb;llvm_ad_rgb" CACHE STRING "Default Mitsuba variants that should be included if no mitsuba.conf file exists")
  mark_as_advanced(MTS_DEFAULT_VARIANTS)
  foreach(TMP ${MTS_DEFAULT_VARIANTS})
    string(APPEND INSERT_CONFIGURATIONS_HERE "\"${TMP}\", ")
  endforeach()
  string(REGEX REPLACE ", $" "" INSERT_CONFIGURATIONS_HERE "${INSERT_CONFIGURATIONS_HERE}")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/mitsuba.conf.template
                 ${CMAKE_CURRENT_BINARY_DIR}/mitsuba.conf @ONLY)
  unset(INSERT_CONFIGURATIONS_HERE)
  set(MTS_COPIED_CONFIG_FILE 1)
endif()

# ----------------------------------------------------------
#  Parse 'mitsuba.conf' and generate derived files:
#   1. include/core/config.h
#   2. python/mitsuba/config.py
# ----------------------------------------------------------

if (NOT Python_EXECUTABLE)
  # We require Python for the next step, even if Python bindings are deactivated
  find_package(Python COMPONENTS Interpreter REQUIRED)
endif()

execute_process(
  COMMAND ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/configure.py ${CMAKE_CXX_COMPILER_ID}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  OUTPUT_VARIABLE MTS_VARIANTS
  ERROR_VARIABLE MTS_VARIANTS_ERR
)

if (MTS_VARIANTS_ERR)
  message(FATAL_ERROR "Could not run resources/configure.py script: ${MTS_VARIANTS_ERR}")
endif()

# ----------------------------------------------------------
#  Check desired variants and set relevant feature flags
# ----------------------------------------------------------

list(LENGTH MTS_VARIANTS MTS_VARIANTS_COUNT)

message(STATUS "Mitsuba: building the following variants:")
foreach (MTS_VARIANT ${MTS_VARIANTS})
  string(REPLACE "|" ";" MTS_VARIANT ${MTS_VARIANT})
  list(GET MTS_VARIANT 0 MTS_VARIANT_NAME)
  list(GET MTS_VARIANT 1 MTS_VARIANT_FLOAT)
  list(GET MTS_VARIANT 2 MTS_VARIANT_SPECTRUM)
  set(MTS_VARIANT_NAMES ${MTS_VARIANT_NAMES} ${MTS_VARIANT_NAME})
  message(STATUS " * ${MTS_VARIANT_NAME}")
endforeach()

string(REPLACE ";" " " MTS_VARIANT_NAMES_STR "${MTS_VARIANT_NAMES}")

if (MTS_VARIANTS MATCHES "cuda_")
  set(MTS_ENABLE_CUDA ON)
endif()

if (MTS_VARIANTS MATCHES "llvm_")
  set(MTS_ENABLE_LLVM ON)
endif()

if (MTS_ENABLE_LLVM OR MTS_ENABLE_CUDA)
  set(MTS_ENABLE_JIT ON)
endif()

if (MTS_VARIANTS MATCHES "ad_")
  set(MTS_ENABLE_AUTODIFF ON)
endif()

# ----------------------------------------------------------
#  Re-run CMake when 'mitsuba.conf' changes (we must
#  dynamically add or remove targets based on this file).
#  Abuses the 'configure_file' mechanism to achieve this.
# ----------------------------------------------------------

configure_file(${CMAKE_CURRENT_BINARY_DIR}/mitsuba.conf
               ${CMAKE_CURRENT_BINARY_DIR}/ext/unused)


# ----------------------------------------------------------
#  Automatically generate a suitable 'setpath.sh' file
# ----------------------------------------------------------

# Directory for build products
if (MSVC)
  # MSVC: .. with generator expression for build type
  set(MTS_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
else()
  set(MTS_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

set(MTS_COPY_FILES "")

# Function to make a read-only copy of a file
macro (ro_copy IN_FILE OUT_FILE)
  set(OUT_FILE_1 "${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE}")
  set(OUT_FILE_2 "${MTS_BINARY_DIR}/${OUT_FILE}")
  if (UNIX)
    add_custom_command(
      OUTPUT ${OUT_FILE_2} DEPENDS ${IN_FILE} ${ARGN}
      COMMAND ${CMAKE_COMMAND} -E copy ${IN_FILE} ${OUT_FILE_2} && chmod a=r ${OUT_FILE_2})
    list(APPEND MTS_COPY_FILES ${OUT_FILE_2})
  else()
    # add_custom_command does not support generator expressions in OUTPUT argument (grr..)
    # make a dummy copy and then depend on that
    add_custom_command(
      OUTPUT ${OUT_FILE_1} DEPENDS ${IN_FILE} ${ARGN}
      COMMAND ${CMAKE_COMMAND} -E copy ${IN_FILE} ${OUT_FILE_1}
      COMMAND ${CMAKE_COMMAND} -E copy ${IN_FILE} ${OUT_FILE_2}
    )
    list(APPEND MTS_COPY_FILES ${OUT_FILE_1})
  endif()
endmacro()

if (NOT SKBUILD)
  if (NOT MSVC)
      configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/setpath.sh
                     ${CMAKE_CURRENT_BINARY_DIR}/setpath.sh @ONLY)
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/setpath.sh DESTINATION ".")
  else()
      configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/setpath.sh
                     ${CMAKE_CURRENT_BINARY_DIR}/resources/setpath.sh @ONLY)
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/setpath.sh DESTINATION ".")
      ro_copy(${CMAKE_CURRENT_BINARY_DIR}/resources/setpath.sh setpath.sh)

      configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/setpath.bat
                     ${CMAKE_CURRENT_BINARY_DIR}/resources/setpath.bat @ONLY)
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/resources/setpath.bat DESTINATION ".")
      ro_copy(${CMAKE_CURRENT_BINARY_DIR}/resources/setpath.bat setpath.bat)

      configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/setpath.ps1
                     ${CMAKE_CURRENT_BINARY_DIR}/resources/setpath.ps1 @ONLY)
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/resources/setpath.ps1 DESTINATION ".")
      ro_copy(${CMAKE_CURRENT_BINARY_DIR}/resources/setpath.ps1 setpath.ps1)
  endif()
endif()

# ----------------------------------

# Clang/GCC address sanitizer
if ((MTS_SANITIZE_ADDRESS OR MTS_SANITIZE_MEMORY) AND
    (CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)"))
  # Don't optimize too heavily
  if (U_CMAKE_BUILD_TYPE MATCHES REL)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O1")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -O1")
    add_compile_options(-O1 -fno-optimize-sibling-calls)
  endif()

  add_compile_options(-fno-omit-frame-pointer)

  if (MTS_SANITIZE_ADDRESS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    message(STATUS "Mitsuba: enabling the address sanitizer.")
  endif()

  if (MTS_SANITIZE_MEMORY)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=memory")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=memory")
    message(STATUS "Mitsuba: enabling the memory sanitizer.")
  endif()
endif()

# Adapt to variations in rpath handling on Linux and macOS
if (APPLE)
  set(MTS_ORIGIN "@loader_path")
elseif(UNIX)
  set(MTS_ORIGIN "$ORIGIN")
endif()

set(CMAKE_INSTALL_RPATH "${MTS_ORIGIN};${MTS_ORIGIN}/../enoki")

# Build the dependencies
add_subdirectory(ext)

# Always add the include directories for tinyformat, Enoki and Eigen
include_directories(include
  ${TINYFORMAT_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}/include
)

if (MTS_ENABLE_CUDA)
  message(STATUS "Mitsuba: using OptiX for GPU ray tracing.")
  add_definitions(-DMTS_ENABLE_CUDA=1)
endif()

if (MTS_ENABLE_LLVM)
  add_definitions(-DMTS_ENABLE_LLVM=1)
endif()

if (MTS_ENABLE_EMBREE)
  include_directories(${EMBREE_INCLUDE_DIRS})
  add_definitions(-DMTS_ENABLE_EMBREE=1)
  message(STATUS "Mitsuba: using Embree for CPU ray tracing.")
else()
  message(STATUS "Mitsuba: using builtin implementation for CPU ray tracing.")
endif()

if (MTS_ENABLE_AUTODIFF)
  add_definitions(-DMTS_ENABLE_AUTODIFF=1)
endif()

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  # Hidden default symbol visibility issues with GCC
  set(CMAKE_CXX_VISIBILITY_PRESET "default")
endif()

if (MSVC)
  # Don't complain about not DLL-exporting STL classes
  add_compile_options(/wd4251)

  # Function '..' marked as __forceinline not inlined
  add_compile_options(/wd4714)

  # unreferenced local function has been removed
  add_compile_options(/wd4505)

  # Declaration of type hides class member
  add_compile_options(/wd4458 /wd4459)

  # Check operator precedence for possible error
  add_compile_options(/wd4554)

  # structure was padded due to alignment specifier
  add_compile_options(/wd4324)

  # conditional expression is constant
  add_compile_options(/wd4127)

  # Unreachable code (MSVC frequently warns in functions using "if constexpr")
  add_compile_options(/wd4702)

  # Parallel build on MSVC
  add_compile_options(/MP)

  # Permit many sections in .obj files
  add_compile_options(/bigobj)

  # Don't complain about incompatible modifier on explicit instantiations
  add_compile_options(/wd4910)
endif()

include(TestBigEndian)
test_big_endian(IS_BIG_ENDIAN)
if (IS_BIG_ENDIAN)
  add_definitions(-DBIG_ENDIAN)
else()
  add_definitions(-DLITTLE_ENDIAN)
endif()

# Completely disable this for now
# if (MTS_ENABLE_PROFILER)
#   add_definitions(-DMTS_ENABLE_PROFILER=1)
#   message(STATUS "Mitsuba: sampling profiler enabled.")
# else()
#   message(STATUS "Mitsuba: sampling profiler disabled.")
# endif()

# Get the current working branch
execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Function for creating Mitsuba plugins
function(add_plugin)
  list(GET ARGV 0 TARGET)
  list(REMOVE_AT ARGV 0)
  add_library(${TARGET} SHARED ${ARGV})
  target_link_libraries(${TARGET} PRIVATE mitsuba-core mitsuba-render)
  set_target_properties(${TARGET} PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${MTS_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${MTS_BINARY_DIR}/plugins
    FOLDER plugins/${MTS_PLUGIN_PREFIX}/${TARGET}
  )
  install(
    TARGETS ${TARGET}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins
  )
endfunction(add_plugin)

# Be extra noisy about unintended float->double conversions
if (CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)" AND NOT MTS_VARIANTS MATCHES "double")
  add_compile_options("-Wdouble-promotion")
endif()

# Forwarding of profiler events to external tools
if (MTS_ENABLE_PROFILER)
  if (MTS_PROFILER_ITTNOTIFY)
    include_directories(${ITT_INCLUDE_DIRS})
    add_definitions(-DMTS_ENABLE_ITTNOTIFY=1)
  endif()

  if (MTS_PROFILER_NVTX)
    set(CUDA_TOOLKIT_ROOT_DIR "" CACHE STRING "Path to the CUDA installation root directory.")
    if (NOT CUDA_TOOLKIT_ROOT_DIR)
      message(FATAL_ERROR "Specify CUDA_TOOLKIT_ROOT_DIR.")
    endif()
    include_directories(${CUDA_TOOLKIT_ROOT_DIR}/include)
    add_definitions(-DMTS_ENABLE_NVTX=1)
  endif()
endif()

# Register the Mitsuba codebase
add_subdirectory(src)

# Documentation
if (NOT SKBUILD)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/cmake")
  find_package(Sphinx)
  if (Sphinx_FOUND)
    set(SPHINX_INPUT_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/docs")
    set(SPHINX_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/html")

    add_custom_target(mkdoc
        ${SPHINX_EXECUTABLE} -b html "${SPHINX_INPUT_DIR}" "${SPHINX_OUTPUT_DIR}"
        COMMENT "Building HTML documentation with Sphinx"
        USES_TERMINAL)

    set(SPHINX_INPUT_DIR_API  "${CMAKE_CURRENT_SOURCE_DIR}/docs/docs_api")
    set(SPHINX_OUTPUT_DIR_API "${CMAKE_CURRENT_BINARY_DIR}/html_api")

    add_custom_target(mkdoc-api
        ${SPHINX_EXECUTABLE} -b html "${SPHINX_INPUT_DIR_API}" "${SPHINX_OUTPUT_DIR_API}"
        COMMENT "Building HTML documentation with Sphinx"
        DEPENDS copy-python
        USES_TERMINAL)
  endif()
endif()

if (MSVC)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT mitsuba)
endif()

# Warn if a mitsuba.conf file was newly created
if (MTS_COPIED_CONFIG_FILE AND NOT SKBUILD)
  message(WARNING "\n${BoldRed}Created a default 'mitsuba.conf' configuration "
          "file. You will probably want to edit this file to specify the "
          "desired configurations before starting to compile.${ColorReset}")
endif()

# Installation targets
set(MTS_DEPEND
  IlmImf IlmThread Imath Iex IexMath Half pugixml
)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64)|(amd64)|(AMD64)")
  set(MTS_DEPEND ${MTS_DEPEND} asmjit)
endif()

if (WIN32 OR APPLE)
  list(APPEND MTS_DEPEND png jpeg)

  if (WIN32)
    list(APPEND MTS_DEPEND zlib)
  endif()
endif()

if (MTS_ENABLE_EMBREE)
  list(APPEND MTS_DEPEND embree)
endif()

if (NOT SKBUILD)
  list(APPEND MTS_DEPEND enoki-thread)

  if (MTS_ENABLE_JIT)
    list(APPEND MTS_DEPEND enoki-jit)
  endif()

  if (MTS_ENABLE_AUTODIFF)
    list(APPEND MTS_DEPEND enoki-autodiff)
  endif()
endif()

# Set up location for build products
set_target_properties(mitsuba mitsuba-core mitsuba-render ${MTS_DEPEND}
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${MTS_BINARY_DIR}
  LIBRARY_OUTPUT_DIRECTORY ${MTS_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${MTS_BINARY_DIR}
  LIBRARY_OUTPUT_DIRECTORY_RELEASE ${MTS_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${MTS_BINARY_DIR}
  LIBRARY_OUTPUT_DIRECTORY_DEBUG ${MTS_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_RELNODEBINFO ${MTS_BINARY_DIR}
  LIBRARY_OUTPUT_DIRECTORY_RELNODEBINFO ${MTS_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${MTS_BINARY_DIR}
  LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${MTS_BINARY_DIR}
)

install(
  TARGETS mitsuba mitsuba-render mitsuba-core ${MTS_DEPEND}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/mitsuba
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  FILES ${TINYFORMAT_INCLUDE_DIRS}/tinyformat.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/include/mitsuba/core/config.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mitsuba/core
)

ro_copy(${CMAKE_CURRENT_BINARY_DIR}/ext/rgb2spec/srgb.coeff data/srgb.coeff rgb2spec_opt_run)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/ext/rgb2spec/srgb.coeff
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}
)

add_custom_target(copy-resources ALL DEPENDS ${MTS_COPY_FILES})

file(GLOB IOR_FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/data/ior/*spd")
add_custom_command(
    TARGET copy-resources DEPENDS IOR_FILES
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/data/ior"
    "${MTS_BINARY_DIR}/data/ior"
)
install(
  DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data/ior
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}
)
