# This is the main CMake build file used to compile Mitsuba
cmake_minimum_required (VERSION 3.9.0)
project(mitsuba)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/resources")

# ---------- Setup mitsuba.conf ----------

if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mitsuba.conf)
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources/mitsuba.conf.template
       DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
  file(RENAME mitsuba.conf.template mitsuba.conf)
  set(MITSUBA_COPIED_CONFIG_FILE 1)
endif()

# ---------- Check for submodules ----------

if (NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ext/openexr/OpenEXR")
  message(FATAL_ERROR "The Mitsuba dependencies are missing! "
    "You probably did not clone the project with --recursive. It is possible to recover "
    "by invoking\n$ git submodule update --init --recursive")
endif()

# ---------- Detect Python ----------

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ext/pybind11/tools")
set(Python_ADDITIONAL_VERSIONS 3.9 3.8 3.7 3.6 3.5)
find_package(PythonLibsNew REQUIRED)


# ---------- Detect renderer variants ----------

# We want CMake to re-run whenever 'mitsuba.conf' changes because the build
# system dynamically adds or removes build targets based on this file. There
# unfortunately isn't a nice way of specifying such dependencies in CMake, but
# it turns out that one can abuse the 'configure_file' mechanism to this end
# (we simply ignore the output file created by it.)
configure_file(mitsuba.conf ${CMAKE_CURRENT_BINARY_DIR}/ext_build/unused)

# Generate the config.h headers from the configuration
execute_process(
  COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/resources/configure.py ${CMAKE_CXX_COMPILER_ID}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE MTS_VARIANTS
  ERROR_VARIABLE MTS_VARIANTS_ERR
)
if (MTS_VARIANTS_ERR)
  message(FATAL_ERROR "Could not run resources/configure.py script: ${MTS_VARIANTS_ERR}")
endif()

list(LENGTH MTS_VARIANTS MTS_VARIANTS_COUNT)

message(STATUS "Building the following variants of Mitsuba:")
foreach (MTS_VARIANT ${MTS_VARIANTS})
  string(REPLACE "|" ";" MTS_VARIANT ${MTS_VARIANT})
  list(GET MTS_VARIANT 0 MTS_VARIANT_NAME)
  list(GET MTS_VARIANT 1 MTS_VARIANT_FLOAT)
  list(GET MTS_VARIANT 2 MTS_VARIANT_SPECTRUM)
  message(STATUS " * ${MTS_VARIANT_NAME}")
endforeach()

if (MTS_VARIANTS MATCHES "gpu_")
  set(MTS_ENABLE_OPTIX ON)
endif()

# ----------------------------------

# ---------- User options ----------

option(MTS_ENABLE_PYTHON  "Build Python bindings for Mitsuba, Enoki, and NanoGUI?" ON)
option(MTS_ENABLE_EMBREE  "Use Embree for ray tracing operations?" OFF)
option(MTS_ENABLE_GUI     "Build GUI" OFF)
if (MTS_ENABLE_OPTIX)
  option(MTS_USE_OPTIX_HEADERS "Use OptiX header files instead of resolving GPU ray tracing API ourselves." OFF)
endif()

if (UNIX)
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

if (MTS_ENABLE_OPTIX AND MTS_USE_OPTIX_HEADERS)
  set(MTS_OPTIX_PATH "/opt/optix" CACHE STRING "Path to OptiX installation")
endif()

# ----------------------------------

if (POLICY CMP0022)
  cmake_policy(SET CMP0022 NEW) # New-style link interface
endif()

if (POLICY CMP0056)
  cmake_policy(SET CMP0056 NEW) # try_compile: pass linker flags to compiler
endif()

if (POLICY CMP0058)
  cmake_policy(SET CMP0058 NEW) # Ninja requires custom command byproducts to be explicit.
endif()

if (POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # MACOSX_RPATH is enabled by default
endif()
set(CMAKE_MACOSX_RPATH ON)

if (POLICY CMP0068)
  cmake_policy(SET CMP0068 NEW) # RPATH settings on macOS don't affect install_name.
endif()

include(CheckCXXCompilerFlag)
include(CheckCXXSourceRuns)
include(TestBigEndian)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/cmake")

# Set a default build configuration (Release)
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

macro(CHECK_CXX_COMPILER_AND_LINKER_FLAGS _RESULT _CXX_FLAGS _LINKER_FLAGS)
  set(CMAKE_REQUIRED_FLAGS ${_CXX_FLAGS})
  set(CMAKE_REQUIRED_LIBRARIES ${_LINKER_FLAGS})
  set(CMAKE_REQUIRED_QUIET TRUE)
  check_cxx_source_runs("#include <iostream>\nint main(int argc, char **argv) { std::cout << \"test\"; return 0; }" ${_RESULT})
  set(CMAKE_REQUIRED_FLAGS "")
  set(CMAKE_REQUIRED_LIBRARIES "")
endmacro()

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

# Prefer libc++ in conjunction with Clang
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


# Clang/GCC address sanitizer
if ((MTS_SANITIZE_ADDRESS OR MTS_SANITIZE_MEMORY) AND (CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)"))
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

if (UNIX AND NOT APPLE)
  # Fix to point the compiler to a header file (__cxxabi_config.h)
  # on recent versions of Debian/Ubuntu
  include_directories(/usr/include/libcxxabi)
endif()

# Build the dependencies
add_subdirectory(ext ext_build)

# Always add the include directories for tinyformat, Enoki and Eigen
include_directories(include
  ${TINYFORMAT_INCLUDE_DIRS}
  ${EIGEN_INCLUDE_DIRS}
  ${ENOKI_INCLUDE_DIRS}
  ${TBB_INCLUDE_DIRS}
)

if (MTS_ENABLE_EMBREE)
  include_directories(${EMBREE_INCLUDE_DIRS})
  add_definitions(-DMTS_ENABLE_EMBREE=1)
  message(STATUS "Mitsuba: using Embree for CPU ray tracing.")
else()
  message(STATUS "Mitsuba: using builtin implementation for CPU ray tracing.")
endif()

if (MTS_ENABLE_OPTIX)
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
  add_definitions(-DMTS_ENABLE_OPTIX=1)
endif()

# Compile with compiler warnings turned on
if (MSVC)
  if (${MSVC_VERSION} LESS 1924)
    message(FATAL_ERROR "MSVC 1924 or higher is required. You are running version ${MSVC_VERSION}.")
  endif()
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    add_compile_options("/W4")
  endif()
else()
  add_compile_options("-Wall" "-Wextra")
  add_compile_options("-Wno-unused-local-typedefs")
  if (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    # Quench inlining-related remarks
    add_compile_options("-wd11074" "-wd11076")
  endif()
endif()

# Language and optimization flags that are used to compile Mitsuba
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|Intel")
  CHECK_CXX_COMPILER_FLAG("-std=gnu++17" HAS_GNUPP17_FLAG)
  CHECK_CXX_COMPILER_FLAG("-std=c++17" HAS_CPP17_FLAG)

  if (HAS_GNUPP17_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17")
  elseif (HAS_CPP17_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
  else()
    message(FATAL_ERROR "Unsupported compiler -- Mitsuba requires C++17 support!")
  endif()

  # Set the default symbol visibility to hidden (very important to obtain small binaries)
  if (U_CMAKE_BUILD_TYPE MATCHES REL)
    # Increase function size limits for inlining on GCC (useful for templated
    # code where significant portions can be optimized away during inlining)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
      add_compile_options("-finline-limit=150")
    endif()

    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU")
      # Set the default symbol visibility to hidden except on GCC
      # which has some issues with extern partial template specialization
      add_compile_options("-fvisibility=hidden")
    endif()
  endif()
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
  # Compile in C++17 mode
  add_compile_options("/std:c++17")

  # Disable annoying MSVC warnings (all targets)
  add_definitions(/D "_CRT_SECURE_NO_WARNINGS" /D "_SCL_SECURE_NO_WARNINGS" /D "_SILENCE_CXX17_UNCAUGHT_EXCEPTION_DEPRECATION_WARNING")

  # Disable secure SCL
  add_definitions(/D "_SECURE_SCL=0")

  # Don't issue implicit linking pragmas for TBB
  add_definitions(/D "__TBB_NO_IMPLICIT_LINKAGE")

  # Disable problematic windows.h min/max macros
  add_definitions(/D "NOMINMAX")

  # Don't complain about not DLL-exporting STL classes
  add_compile_options("/wd4251")

  # Function '..' marked as __forceinline not inlined
  add_compile_options("/wd4714")

  # unreferenced local function has been removed
  add_compile_options("/wd4505")

  # Declaration of type hides class member
  add_compile_options("/wd4458" "/wd4459")

  # Check operator precedence for possible error
  add_compile_options("/wd4554")

  # structure was padded due to alignment specifier
  add_compile_options("/wd4324")

  # conditional expression is constant
  add_compile_options("/wd4127")

  # Workaround for MSVC 2017
  add_compile_options("/wd4244")

  # Parallel build on MSVC
  add_compile_options("/MP")

  # Permit many sections in .obj files
  add_compile_options("/bigobj")

  # Use defines from math.h (M_PI, etc)
  add_compile_options(/D "_USE_MATH_DEFINES")

  # Don't complain about incompatible modifier on explicit instantiations
  add_compile_options("/wd4910")
endif()

if (U_CMAKE_BUILD_TYPE MATCHES DEBUG AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options("-fno-limit-debug-info")
endif()

# Enable link time optimization in release mode (if possible)
if (U_CMAKE_BUILD_TYPE MATCHES REL AND CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)")
  if (NOT MTS_ENABLE_LTO)
    message(STATUS "Mitsuba: LTO support disabled (MTS_ENABLE_LTO=OFF).")
  elseif (NOT CMAKE_CXX_FLAGS MATCHES "-flto")
    # Enable link time optimization
    set(BACKUP_C_FLAGS ${CMAKE_CXX_FLAGS})
    set(BACKUP_CXX_FLAGS ${CMAKE_C_FLAGS})
    set(BACKUP_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS})
    set(BACKUP_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS})

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      set(CMAKE_CXX_FLAGS "-flto=thin ${CMAKE_CXX_FLAGS}")
      set(CMAKE_C_FLAGS "-flto=thin ${CMAKE_C_FLAGS}")

      file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/.cache)
      if (APPLE)
        set(CMAKE_EXE_LINKER_FLAGS    "-flto=thin -Wl,-cache_path_lto,${CMAKE_CURRENT_BINARY_DIR}/.cache ${CMAKE_EXE_LINKER_FLAGS}")
        set(CMAKE_SHARED_LINKER_FLAGS "-flto=thin -Wl,-cache_path_lto,${CMAKE_CURRENT_BINARY_DIR}/.cache ${CMAKE_SHARED_LINKER_FLAGS}")
      else()
        set(CMAKE_EXE_LINKER_FLAGS    "-flto=thin -Wl,-plugin-opt,cache-dir=${CMAKE_CURRENT_BINARY_DIR}/.cache ${CMAKE_EXE_LINKER_FLAGS}")
        set(CMAKE_SHARED_LINKER_FLAGS "-flto=thin -Wl,-plugin-opt,cache-dir=${CMAKE_CURRENT_BINARY_DIR}/.cache ${CMAKE_SHARED_LINKER_FLAGS}")
      endif()

      if (NOT APPLE AND U_CMAKE_BUILD_TYPE MATCHES MINSIZEREL)
          # Clang Gold plugin does not support -Os
          set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O3")
          set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -O3")
      endif()
    else()
      set(CMAKE_EXE_LINKER_FLAGS "-flto ${CMAKE_EXE_LINKER_FLAGS}")
      set(CMAKE_SHARED_LINKER_FLAGS "-flto ${CMAKE_SHARED_LINKER_FLAGS}")
      set(CMAKE_CXX_FLAGS "-flto -fno-fat-lto-objects ${CMAKE_CXX_FLAGS}")
      set(CMAKE_C_FLAGS "-flto -fno-fat-lto-objects ${CMAKE_C_FLAGS}")
    endif()

    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/CMakeTmp/test.c "int main(int argc, char **argv) { return 0; }")
    try_compile(HAS_LTO
      ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/CMakeTmp
      ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/CMakeTmp/test.c)

    if (HAS_LTO)
      message(STATUS "Mitsuba: LTO support enabled.")
    else()
      message(STATUS "Mitsuba: LTO not supported by the compiler.")
      set(CMAKE_C_FLAGS ${BACKUP_CXX_FLAGS})
      set(CMAKE_CXX_FLAGS ${BACKUP_C_FLAGS})
      set(CMAKE_EXE_LINKER_FLAGS ${BACKUP_EXE_LINKER_FLAGS})
      set(CMAKE_SHARED_LINKER_FLAGS ${BACKUP_SHARED_LINKER_FLAGS})
    endif()
  endif()
elseif(MSVC)
  set(Configurations RELEASE RELWITHDEBINFO MINSIZEREL)
  set(LinkTypes EXE SHARED MODULE STATIC)
  foreach(Configuration ${Configurations})
    set(FLAGS ${CMAKE_CXX_FLAGS_${Configuration}})

    if (NOT FLAGS MATCHES "/GL")
      set(FLAGS "${FLAGS} /GL")
    endif()

    set("CMAKE_CXX_FLAGS_${Configuration}" "${FLAGS}" CACHE STRING "" FORCE)

    foreach(LinkType ${LinkTypes})
      set(FLAGS "${CMAKE_${LinkType}_LINKER_FLAGS_${Configuration}}")

      if (NOT FLAGS MATCHES "/LTCG")
        set(FLAGS "${FLAGS} /LTCG")
      endif()

      if ((LinkType STREQUAL "EXE" OR LinkType STREQUAL "SHARED") AND NOT (Configuration MATCHES DEB) AND NOT FLAGS MATCHES "/OPT:REF,ICF")
        set(FLAGS "${FLAGS} /OPT:REF,ICF")
      endif()

      set("CMAKE_${LinkType}_LINKER_FLAGS_${Configuration}" "${FLAGS}" CACHE STRING "" FORCE)
    endforeach()
  endforeach()

  foreach(LinkType EXE SHARED)
    string(REGEX REPLACE "\/INCREMENTAL(\:NO)?" "" CMAKE_${LinkType}_LINKER_FLAGS_DEBUG "${CMAKE_${LinkType}_LINKER_FLAGS_DEBUG}")
    set("CMAKE_${LinkType}_LINKER_FLAGS_DEBUG" "${CMAKE_${LinkType}_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
  endforeach()

  message(STATUS "Mitsuba: LTO support enabled.")
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
  add_definitions(-DMTS_ENABLE_PROFILER)
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

# Schedules a file to be copied to the 'dist' directory
function(add_dist)
  set(MITSUBA_DIST ${MITSUBA_DIST} ${ARGV} CACHE INTERNAL "")
endfunction()

# Registers a test directory to be run by the `pytest` command
function(add_tests)
  file(GLOB TEST_FILES ${ARGV}/test_*)
  set(MITSUBA_TEST_DIRECTORIES ${MITSUBA_TEST_DIRECTORIES} ${TEST_FILES} CACHE INTERNAL "")
endfunction()

# Function for creating Mitsuba plugins
function(add_plugin)
  list(GET ARGV 0 TARGET)
  list(REMOVE_AT ARGV 0)
  add_library(${TARGET}-obj OBJECT ${ARGV})
  add_library(${TARGET} SHARED $<TARGET_OBJECTS:${TARGET}-obj>)
  set_property(TARGET ${TARGET} PROPERTY POSITION_INDEPENDENT_CODE ON)
  set_property(TARGET ${TARGET}-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
  set_target_properties(${TARGET} PROPERTIES PREFIX "")
  target_link_libraries(${TARGET} PRIVATE mitsuba-core mitsuba-render tbb)
  add_dist(plugins/${TARGET})
  set_target_properties(${TARGET} ${TARGET}-obj PROPERTIES FOLDER plugins/${MTS_PLUGIN_PREFIX}/${TARGET})
endfunction(add_plugin)

# Initialize CMake variables
set(MITSUBA_DIST "" CACHE INTERNAL "")
set(MITSUBA_TEST_DIRECTORIES "" CACHE INTERNAL "")

# Rpath handling for OSX and Linux
if (APPLE)
  set(CMAKE_BUILD_RPATH "@loader_path")
  SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
elseif (UNIX)
  SET(CMAKE_INSTALL_RPATH "\$ORIGIN")
  SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)" AND NOT MTS_VARIANTS MATCHES "double")
  # Be extra noisy about unintended float->double conversions
  add_compile_options("-Wdouble-promotion")
endif()

# Register the Mitsuba codebase
add_subdirectory(src)

# Copy dependencies into the 'dist' directory
foreach(ITEM ${MITSUBA_DIST})
  get_filename_component(TARGET_NAME ${ITEM} NAME)
  get_filename_component(TARGET_DIRECTORY ${ITEM} DIRECTORY)
  get_target_property(TARGET_OUTPUT_NAME ${TARGET_NAME} OUTPUT_NAME)
  get_target_property(TARGET_SUFFIX ${TARGET_NAME} SUFFIX)
  get_target_property(TARGET_PREFIX ${TARGET_NAME} PREFIX)
  if (TARGET_OUTPUT_NAME MATCHES "NOTFOUND")
    set(TARGET_OUTPUT_NAME ${TARGET_NAME})
  endif()
  if (TARGET_PREFIX MATCHES "NOTFOUND")
    get_target_property(TARGET_TYPE ${TARGET_NAME} TYPE)
    set(TARGET_PREFIX ${CMAKE_${TARGET_TYPE}_PREFIX})
  endif()
  if (TARGET_SUFFIX MATCHES "NOTFOUND")
    get_target_property(TARGET_TYPE ${TARGET_NAME} TYPE)
    set(TARGET_SUFFIX ${CMAKE_${TARGET_TYPE}_SUFFIX})
  endif()
  set(TARGET_FILENAME ${TARGET_PREFIX}${TARGET_OUTPUT_NAME}${TARGET_SUFFIX})
  if (TARGET_DIRECTORY STREQUAL "")
    set(TARGET_DIRECTORY ".")
  endif()
  set(OUTPUT_FILE ${CMAKE_BINARY_DIR}/dist/${TARGET_DIRECTORY}/${TARGET_FILENAME})
  add_custom_command(
    OUTPUT ${OUTPUT_FILE} DEPENDS ${TARGET_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET_NAME}> ${OUTPUT_FILE})
  list(APPEND MITSUBA_DIST_OUT ${OUTPUT_FILE})
endforeach()

macro (add_file DST SRC)
  set(OUTPUT_FILE ${CMAKE_BINARY_DIR}/dist/${DST})
  add_custom_command(
    OUTPUT ${OUTPUT_FILE} DEPENDS ${SRC} ${ARGN}
    COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${OUTPUT_FILE})
  list(APPEND MITSUBA_DIST_OUT ${OUTPUT_FILE})
endmacro()

add_file(data/srgb.coeff ${CMAKE_BINARY_DIR}/ext_build/rgb2spec/srgb.coeff rgb2spec_opt_run)

file(COPY resources/data/ior DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/dist/data)

add_custom_target(dist-copy ALL DEPENDS ${MITSUBA_DIST_OUT})
set_property(TARGET dist-copy PROPERTY FOLDER misc)

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

if(NOT WIN32)
  string(ASCII 27 Esc)
  set(ColorReset "${Esc}[m")
  set(BoldRed "${Esc}[1;31m")
endif()

if (MITSUBA_COPIED_CONFIG_FILE)
  message(WARNING "\n${BoldRed}Created a default 'mitsuba.conf' configuration "
          "file. You will probably want to edit this file to specify the "
          "desired configurations before starting to compile.${ColorReset}")
endif()
