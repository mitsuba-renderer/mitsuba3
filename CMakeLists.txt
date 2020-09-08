# ----------------------------------------------------------
# Top-level Mitsuba 2 CMake file, requires a recent version.
# ----------------------------------------------------------
cmake_minimum_required (VERSION 3.13..3.18)

project(mitsuba
  DESCRIPTION
    "Mitsuba Renderer 2"
  LANGUAGES
    CXX
)

# A number of Mitsuba 2 features (scripting, differentiable rendering, testing
# infrastructure, etc.) are only available if Python bindings are built.
option(MTS_ENABLE_PYTHON  "Build Python bindings for Mitsuba, Enoki, and NanoGUI?" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/cmake")

# ----------------------------------------------------------
#  In-tree builds are not permitted
# ----------------------------------------------------------

if (PROJECT_SOURCE_DIR STREQUAL "${PROJECT_BINARY_DIR}")
  message(FATAL_ERROR "In-tree builds are not permitted.")
endif()

# ----------------------------------------------------------
#  Copy mitsuba.conf if not found
# ----------------------------------------------------------

if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/mitsuba.conf)
  file(COPY resources/mitsuba.conf.template DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/mitsuba.conf.template
              ${CMAKE_CURRENT_BINARY_DIR}/mitsuba.conf)
  set(MTS_COPIED_CONFIG_FILE 1)
endif()

# ----------------------------------------------------------
#  Check if submodules have been checked out, or fail early
# ----------------------------------------------------------

if (NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ext/openexr/OpenEXR")
  message(FATAL_ERROR "The Mitsuba dependencies are missing! "
    "You probably did not clone the project with --recursive. It is possible to recover "
    "by invoking\n$ git submodule update --init --recursive")
endif()

# ----------------------------------------------------------
#  We require Python in the following
# ----------------------------------------------------------

if (MTS_ENABLE_PYTHON)
  find_package(Python COMPONENTS Interpreter Development REQUIRED)

  add_subdirectory(ext/pybind11)
  mark_as_advanced(
    PYBIND11_FINDPYTHON
    PYBIND11_NOPYTHON)
else()
  find_package(Python COMPONENTS Interpreter REQUIRED)
endif()

# ----------------------------------------------------------
#  Re-run CMake when 'mitsuba.conf' changes (we must
#  dynamically add or remove targets based on this file).
#  Abuses the 'configure_file' mechanism to achieve this..
# ----------------------------------------------------------

configure_file(${CMAKE_CURRENT_BINARY_DIR}/mitsuba.conf
               ${CMAKE_CURRENT_BINARY_DIR}/ext/unused)

# Generate the config.h headers from the configuration
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

message(STATUS "Building the following variants of Mitsuba:")
foreach (MTS_VARIANT ${MTS_VARIANTS})
  string(REPLACE "|" ";" MTS_VARIANT ${MTS_VARIANT})
  list(GET MTS_VARIANT 0 MTS_VARIANT_NAME)
  list(GET MTS_VARIANT 1 MTS_VARIANT_FLOAT)
  list(GET MTS_VARIANT 2 MTS_VARIANT_SPECTRUM)
  message(STATUS " * ${MTS_VARIANT_NAME}")
endforeach()

if (MTS_VARIANTS MATCHES "cuda_")
  set(MTS_ENABLE_CUDA ON)
endif()

if (MTS_VARIANTS MATCHES "llvm_")
  set(MTS_ENABLE_LLVM ON)
endif()

if (MTS_VARIANTS MATCHES "autodiff_")
  set(MTS_ENABLE_AUTODIFF ON)
endif()

if (MTS_ENABLE_LLVM OR MTS_ENABLE_CUDA)
  set(MTS_ENABLE_JIT ON)
endif()

# ----------------------------------------------------------
#  Optional features available to users
# ----------------------------------------------------------

# Compile for the native processor architecture by default. MSVC cannot detect
# this automatically, and the following assumes a reasonably modern CPU with
# AVX2 instructions. You may want to overwrite MTS_NATIVE_FLAGS when building
# binaries that will be distributed to others.

if (MSVC)
  set(MTS_NATIVE_FLAGS_DEFAULT "/arch:AVX2")
else()
  set(MTS_NATIVE_FLAGS_DEFAULT "-march=native")
endif()

set(MTS_NATIVE_FLAGS ${MTS_NATIVE_FLAGS_DEFAULT} CACHE STRING
    "Compilation flags used to target the host processor architecture.")

# Mitsuba 2 ships with its own ray tracing acceleration data structure. If
# desired, Embree (which tends to be quite a bit faster) can be used instead.
# Embree lacks some features provided by Mitsuba 2, like support for double
# precision arithmetic.
option(MTS_ENABLE_EMBREE  "Use Embree for ray tracing operations?" ON)

# Build the graphical user interface? (Note that this feature remains incomplete)
option(MTS_ENABLE_GUI     "Build the graphical user interface?" OFF)

if (MTS_ENABLE_CUDA)
  # Mitsuba 2 resolves OptiX dynamically and can thus be compiled even if the official headers
  # aren't installed. In very rare cases, it may be helpful to disable this dynamic behavior.
  option(MTS_USE_OPTIX_HEADERS "Compile against OptiX headers, instead of resolving API dynamically." OFF)
endif()

if (UNIX)
  # Mitsuba 2 ships with a built-in sampling profiler that highlights where
  # rendering time is spent. Only supported on Linux/macOS at the moment.
  option(MTS_ENABLE_PROFILER "Enable sampling profiler" ON)
endif()

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

# For developers: ability to disable Link Time Optimization to speed up builds
option(MTS_ENABLE_LTO "Enable Link Time Optimization (LTO)?" ON)

if (MTS_ENABLE_CUDA AND MTS_USE_OPTIX_HEADERS)
  set(MTS_OPTIX_PATH "/opt/optix" CACHE STRING "Path to OptiX installation")
endif()

# ----------------------------------

include(CheckCXXCompilerFlag)
include(CheckCXXSourceRuns)
include(TestBigEndian)


# ----------------------------------------------------------
#  Set a default build configuration (Release)
# ----------------------------------------------------------

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

# ----------------------------------------------------------
#  Compiler version check
# ----------------------------------------------------------

if (CMAKE_CXX_COMPILER_ID MATCHES "^(GNU)$")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
     message(FATAL_ERROR "Your version of GCC is too old (found version ${CMAKE_CXX_COMPILER_VERSION}. Please use at least GCC 8.0)")
  endif()
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
    message(FATAL_ERROR "Your version of Clang is too old (found version ${CMAKE_CXX_COMPILER_VERSION}. Please use at least Clang 7.0)")
  endif()
endif()

if (MSVC)
  if (${MSVC_VERSION} LESS 1924)
    message(FATAL_ERROR "MSVC 1924 or higher is required. You are running version ${MSVC_VERSION}.")
  endif()
endif()

# ----------------------------------------------------------
#  Use libc++ together with Clang
# ----------------------------------------------------------

macro(CHECK_CXX_COMPILER_AND_LINKER_FLAGS _RESULT _CXX_FLAGS _LINKER_FLAGS)
  set(CMAKE_REQUIRED_FLAGS ${_CXX_FLAGS})
  set(CMAKE_REQUIRED_LIBRARIES ${_LINKER_FLAGS})
  set(CMAKE_REQUIRED_QUIET TRUE)
  check_cxx_source_runs("#include <iostream>\nint main(int argc, char **argv) { std::cout << \"test\"; return 0; }" ${_RESULT})
  set(CMAKE_REQUIRED_FLAGS "")
  set(CMAKE_REQUIRED_LIBRARIES "")
endmacro()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_FLAGS MATCHES "-stdlib=libc\\+\\+")
  CHECK_CXX_COMPILER_AND_LINKER_FLAGS(HAS_LIBCPP "-stdlib=libc++" "-stdlib=libc++")
  if (HAS_LIBCPP)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -D_LIBCPP_VERSION")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
    message(STATUS "Mitsuba: using libc++.")
  else()
    CHECK_CXX_COMPILER_AND_LINKER_FLAGS(HAS_LIBCPP_AND_CPPABI "-stdlib=libc++" "-stdlib=libc++ -lc++abi")
    if (HAS_LIBCPP_AND_CPPABI)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -D_LIBCPP_VERSION")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
      message(STATUS "Mitsuba: using libc++ and libc++abi.")
    else()
      message(FATAL_ERROR "When Clang is used to compile Mitsuba, libc++ must be available -- GCC's libstdc++ is not supported! (please install the libc++ development headers, provided e.g. by the packages 'libc++-dev' and 'libc++abi-dev' on Debian/Ubuntu).")
    endif()
  endif()
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MTS_NATIVE_FLAGS}")

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

# Enable folders for projects in Visual Studio
if (CMAKE_GENERATOR MATCHES "Visual Studio")
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

if (NOT WIN32)
  string(ASCII 27 Esc)
  set(ColorReset "${Esc}[m")
  set(BoldRed "${Esc}[1;31m")
endif()

# Build the dependencies
add_subdirectory(ext)

# Always add the include directories for tinyformat, Enoki and Eigen
include_directories(include
  ${TINYFORMAT_INCLUDE_DIRS}
  ${EIGEN_INCLUDE_DIRS}
  ${ENOKI_INCLUDE_DIRS}
  ${TBB_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}/include
)

if (MTS_ENABLE_CUDA)
  if (MTS_USE_OPTIX_HEADERS AND NOT EXISTS "${MTS_OPTIX_PATH}/include/optix.h")
    message(FATAL_ERROR "optix.h not found, run CMake with -DMTS_OPTIX_PATH=...")
  endif()

  if (MTS_USE_OPTIX_HEADERS)
    find_package(CUDA 10.0 REQUIRED)
    include_directories(${MTS_OPTIX_PATH}/include ${CUDA_INCLUDE_DIRS})
    add_definitions(-DMTS_USE_OPTIX_HEADERS=1)
    message(STATUS "Mitsuba: OptiX header files will be used.")
  endif()

  message(STATUS "Mitsuba: using OptiX for GPU ray tracing.")
  add_definitions(-DMTS_ENABLE_CUDA=1)
endif()

if (MTS_ENABLE_LLVM)
  add_definitions(-DMTS_ENABLE_LLVM=1)
  if (NOT MTS_ENABLE_EMBREE)
    message(FATAL_ERROR "Compiling a LLVM variant also requires that Embree is enabled! (-DMTS_ENABLE_EMBREE=1)")
  endif()
endif()

if (MTS_ENABLE_AUTODIFF)
  add_definitions(-DMTS_ENABLE_AUTODIFF=1)
endif()

if (MTS_ENABLE_EMBREE)
  include_directories(${EMBREE_INCLUDE_DIRS})
  add_definitions(-DMTS_ENABLE_EMBREE=1)
  message(STATUS "Mitsuba: using Embree for CPU ray tracing.")
else()
  message(STATUS "Mitsuba: using builtin implementation for CPU ray tracing.")
endif()

set(CMAKE_CXX_STANDARD 17)

# Language and optimization flags that are used to compile Mitsuba
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang"
    AND U_CMAKE_BUILD_TYPE MATCHES REL)
  # Set the default symbol visibility to hidden to reduce binary
  # size--don't do it on GCC, which seems to have some issues with
  # extern partial template specialization
  add_compile_options("-fvisibility=hidden")
endif()

# Force colored output for the ninja generator
if (CMAKE_GENERATOR STREQUAL "Ninja")
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcolor-diagnostics")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
  elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color=always")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
  endif()
endif()

if (MSVC)
  # Disable checked iterators
  add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)

  # Don't issue implicit linking pragmas for TBB
  add_definitions(-D__TBB_NO_IMPLICIT_LINKAGE)

  # Disable problematic windows.h min/max macros
  add_definitions(-DNOMINMAX)

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

# Set platform-specific flags
if (WIN32)
  add_definitions(-D__WINDOWS__)
elseif(UNIX)
  if(APPLE)
    add_definitions(-D__OSX__)
  else()
    add_definitions(-D__LINUX__)
  endif()
endif()

test_big_endian(IS_BIG_ENDIAN)
if (IS_BIG_ENDIAN)
  add_definitions(-DBIG_ENDIAN)
else()
  add_definitions(-DLITTLE_ENDIAN)
endif()

if (MTS_ENABLE_PROFILER)
  add_definitions(-DMTS_ENABLE_PROFILER=1)
  message(STATUS "Mitsuba: sampling profiler enabled.")
else()
  message(STATUS "Mitsuba: sampling profiler disabled.")
endif()

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

set(MTS_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Function for creating Mitsuba plugins
function(add_plugin)
  list(GET ARGV 0 TARGET)
  list(REMOVE_AT ARGV 0)
  add_library(${TARGET} SHARED ${ARGV})
  target_link_libraries(${TARGET} PRIVATE mitsuba-core mitsuba-render tbb)
  set_target_properties(${TARGET} PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${MTS_BINARY_DIR}/plugins
    FOLDER plugins/${MTS_PLUGIN_PREFIX}/${TARGET}
  )
  install(
    TARGETS ${TARGET}
    LIBRARY DESTINATION plugins
  )
endfunction(add_plugin)

# Be extra noisy about unintended float->double conversions
if (CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)" AND NOT MTS_VARIANTS MATCHES "double")
  add_compile_options("-Wdouble-promotion")
endif()

# Adapt to variations in rpath handling on Linux and macOS
if (APPLE)
  set(MTS_ORIGIN "@loader_path")
elseif(UNIX)
  set(MTS_ORIGIN "$ORIGIN")
endif()

# Compile with compiler warnings turned on
if (MSVC)
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    add_compile_options("/W4")
  endif()

  # Disable annoying MSVC security features + warnings
  add_definitions(
    -D_CRT_SECURE_NO_WARNINGS
    -D_SCL_SECURE_NO_WARNINGS
    -D_SILENCE_CXX17_UNCAUGHT_EXCEPTION_DEPRECATION_WARNING
  )
else()
  add_compile_options(-Wall -Wextra -Wno-unused-local-typedefs)
endif()

# Register the Mitsuba codebase
add_subdirectory(src)

# Documentation
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
      DEPENDS mitsuba-python dist-copy
      USES_TERMINAL)
endif()

if (MSVC)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT mitsuba)
endif()

# Warn if a mitsuba.conf file was newly created
if (MTS_COPIED_CONFIG_FILE)
  message(WARNING "\n${BoldRed}Created a default 'mitsuba.conf' configuration "
          "file. You will probably want to edit this file to specify the "
          "desired configurations before starting to compile.${ColorReset}")
endif()

# Warn about Unix Makefiles
if (CMAKE_GENERATOR STREQUAL "Unix Makefiles")
  message(WARNING "\n${BoldRed}You're using CMake's \"Unix Makefiles\" "
    "generator. Compilation speed is going to be impacted due to its limited "
    "ability to express build parallelism. We strongly recommend using Ninja "
    "for development on Linux and macOS. To do so, install \"ninja\" (brew, "
    "MacPorts, Arch) or \"ninja-build\" elsewhere, and then invoke CMake using "
    "\"-GNinja\") ${ColorReset}")
endif()

# Make sure that dynamic libraries still can be found following 'make install'
set_target_properties(mitsuba PROPERTIES INSTALL_RPATH "${MTS_ORIGIN}")

# Installation targets
set(MTS_DEPEND
  asmjit IlmImf IlmThread Imath Iex Half tbb pugixml
)

if (WIN32 OR APPLE)
  list(APPEND MTS_DEPEND png16 jpeg)

  if (WIN32)
    list(APPEND MTS_DEPEND zlib)
  endif()
endif()

if (MTS_ENABLE_EMBREE)
  list(APPEND MTS_DEPEND embree)
endif()

if (MTS_ENABLE_JIT)
  list(APPEND MTS_DEPEND enoki-jit)
  set_target_properties(enoki-jit PROPERTIES
    INSTALL_RPATH "$ORIGIN")
endif()

if (MTS_ENABLE_AUTODIFF)
  list(APPEND MTS_DEPEND enoki-autodiff)
  set_target_properties(enoki-autodiff PROPERTIES
    INSTALL_RPATH "$ORIGIN")
endif()

# Set up location for build products
set_target_properties(mitsuba mitsuba-core mitsuba-render ${MTS_DEPEND}
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${MTS_BINARY_DIR}
  LIBRARY_OUTPUT_DIRECTORY ${MTS_BINARY_DIR}
)

install(
  TARGETS mitsuba mitsuba-render mitsuba-core ${MTS_DEPEND}
  LIBRARY DESTINATION .
  RUNTIME DESTINATION .
)

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/mitsuba
  DESTINATION include
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/include/mitsuba/core/config.h
  DESTINATION include/mitsuba/core
)

if (WIN32)
  install(FILES resources/setpath.bat DESTINATION ".")
  file(COPY resources/setpath.bat DESTINATION ${MTS_BINARY_DIR})
else()
  install(FILES resources/setpath.sh DESTINATION ".")
  file(COPY resources/setpath.sh DESTINATION ${MTS_BINARY_DIR})
endif()


mark_as_advanced(
  ZLIB_BUILD_AMD64
  ZLIB_BUILD_ASM686
  ZLIB_BUILD_EXAMPLES
  ZLIB_BUILD_SHARED_LIBS
  ZLIB_BUILD_STATIC_LIBS
  ZLIB_LIBRARY
  EMBREE_STATIC_RUNTIME
  INSTALL_BIN_DIR
  INSTALL_MAN_DIR
  INSTALL_INC_DIR
  INSTALL_LIB_DIR
  INSTALL_PKGCONFIG_DIR
)
