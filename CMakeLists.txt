# ----------------------------------------------------------
# Top-level Mitsuba 3 CMake file, requires a recent version.
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

# A number of Mitsuba 3 features (scripting, differentiable rendering, testing
# infrastructure, etc.) are only available if Python bindings are built.

option(MI_ENABLE_PYTHON "Build Python bindings for Mitsuba, Dr.Jit, and NanoGUI?" ON)

# Mitsuba 3 ships with its own ray tracing acceleration data structure. If
# desired, Embree (which tends to be quite a bit faster) can be used instead.
# Embree lacks some features provided by Mitsuba 3, like support for double
# precision arithmetic.
option(MI_ENABLE_EMBREE  "Use Embree for ray tracing operations?" ON)

# Use GCC/Clang address sanitizer?
# NOTE: To use this in conjunction with Python plugin, you will need to call
# On OSX:
#   export DYLD_INSERT_LIBRARIES=<path to libclang_rt.asan_osx_dynamic.dylib>
# On Linux:
#   export LD_LIBRARY_PATH=<path to libasan.so>

option(MI_SANITIZE_ADDRESS "Enable GCC/Clang address sanitizer?" OFF) # To catch out-of-bounds accesses
option(MI_SANITIZE_MEMORY  "Enable GCC/Clang memory sanitizer?"  OFF) # To catch use of uninitialized memory

option(MI_THROW_TRAPS_DEBUGGER "Trap the debugger on calls to `Throw`?" OFF)
if(MI_THROW_TRAPS_DEBUGGER)
  add_definitions(-DMI_THROW_TRAPS_DEBUGGER)
endif()

option(MI_PROFILER_ITTNOTIFY "Forward profiler events (to Intel VTune)?" OFF)
option(MI_PROFILER_NVTX      "Forward profiler events (to NVIDIA Nsight)?" OFF)

if (NOT APPLE)
  option(MI_ENABLE_OPTIX_DEBUG_VALIDATION "Enable debug flag for OptiX" OFF)
endif()

option(MI_STABLE_ABI "Build Python extension using the CPython stable ABI? (Only relevant when using scikit-build)" OFF)
mark_as_advanced(MI_STABLE_ABI)

# ----------------------------------------------------------
#  Check if submodules have been checked out, or fail early
# ----------------------------------------------------------

if (NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ext/drjit/ext/drjit-core/ext/nanothread/ext/cmake-defaults")
  message(FATAL_ERROR "The Mitsuba 3 dependencies are missing! "
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
          "  $ rm -Rf CMakeCache.txt CMakeFiles\n"
          "  $ cmake -S . -B build -G Ninja\n"
          "  $ cmake --build build")
endif()

# ----------------------------------------------------------
#  Build defaults for projects by the Realistic Graphics Lab
# ----------------------------------------------------------

set(P "MI") # <-- prefix for CMake variables, used by the following script
set(MI_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/include/mitsuba/mitsuba.h")
include(ext/drjit/ext/drjit-core/ext/nanothread/ext/cmake-defaults/CMakeLists.txt)

# ----------------------------------------------------------
#  scikit-build: find installed version of dependencies
# ----------------------------------------------------------

if (SKBUILD)

  set(CMAKE_INSTALL_LIBDIR mitsuba)
  set(CMAKE_INSTALL_BINDIR mitsuba)
  set(CMAKE_INSTALL_DATAROOTDIR mitsuba/data)
  set(CMAKE_INSTALL_INCLUDEDIR mitsuba/include)

  set(MI_DRJIT_CMAKE_DIR "" CACHE STRING "Location of Dr.Jit's CMake directory that should be used instead of the output of `drjit.get_cmake_dir()` when building with scikit-build.")
  mark_as_advanced(MI_DRJIT_CMAKE_DIR)

  if("${MI_DRJIT_CMAKE_DIR}" STREQUAL "")
    execute_process(
      COMMAND
      "${PYTHON_EXECUTABLE}" -c
      "import drjit; print(drjit.get_cmake_dir())"
      OUTPUT_VARIABLE MI_DRJIT_CMAKE_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ECHO STDOUT)
  endif()

  list(APPEND CMAKE_PREFIX_PATH "${MI_DRJIT_CMAKE_DIR}")
  find_package(drjit CONFIG REQUIRED)
endif()

# ----------------------------------------------------------
#  Create the 'mitsuba.conf' file if none is found
# ----------------------------------------------------------

if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/mitsuba.conf)
  set(MI_DEFAULT_VARIANTS "scalar_rgb,scalar_spectral,cuda_ad_rgb,llvm_ad_rgb,llvm_ad_spectral" CACHE STRING "Default Mitsuba variants that should be included if no mitsuba.conf file exists")
  string(REPLACE "," ";" MI_DEFAULT_VARIANTS "${MI_DEFAULT_VARIANTS}")
  message(STATUS "MI_DEFAULT_VARIANTS: ${MI_DEFAULT_VARIANTS}")
  mark_as_advanced(MI_DEFAULT_VARIANTS)
  foreach(TMP ${MI_DEFAULT_VARIANTS})
    string(APPEND INSERT_CONFIGURATIONS_HERE "\"${TMP}\", ")
  endforeach()
  string(REGEX REPLACE ", $" "" INSERT_CONFIGURATIONS_HERE "${INSERT_CONFIGURATIONS_HERE}")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/mitsuba.conf.template
                 ${CMAKE_CURRENT_BINARY_DIR}/mitsuba.conf @ONLY)
  unset(INSERT_CONFIGURATIONS_HERE)
  set(MI_COPIED_CONFIG_FILE 1)
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
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/configure.py
  ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} ${CMAKE_CURRENT_SOURCE_DIR}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  OUTPUT_VARIABLE MI_VARIANTS
  ERROR_VARIABLE MI_VARIANTS_ERR
)

if (MI_VARIANTS_ERR)
  message(FATAL_ERROR "Could not run resources/configure.py script: ${MI_VARIANTS_ERR}")
endif()

# ----------------------------------------------------------
#  Check desired variants and set relevant feature flags
# ----------------------------------------------------------

list(LENGTH MI_VARIANTS MI_VARIANTS_COUNT)

message(STATUS "Mitsuba: building the following variants:")
foreach (MI_VARIANT ${MI_VARIANTS})
  string(REPLACE "|" ";" MI_VARIANT ${MI_VARIANT})
  list(GET MI_VARIANT 0 MI_VARIANT_NAME)
  list(GET MI_VARIANT 1 MI_VARIANT_FLOAT)
  list(GET MI_VARIANT 2 MI_VARIANT_SPECTRUM)
  set(MI_VARIANT_NAMES ${MI_VARIANT_NAMES} ${MI_VARIANT_NAME})
  message(STATUS " * ${MI_VARIANT_NAME}")
endforeach()

string(REPLACE ";" " " MI_VARIANT_NAMES_STR "${MI_VARIANT_NAMES}")

if (MI_VARIANTS MATCHES "cuda_")
  set(MI_ENABLE_CUDA ON)
endif()

if (MI_VARIANTS MATCHES "llvm_")
  set(MI_ENABLE_LLVM ON)
endif()

if (MI_ENABLE_LLVM OR MI_ENABLE_CUDA)
  set(MI_ENABLE_JIT ON)
endif()

if (MI_VARIANTS MATCHES "ad_")
  set(MI_ENABLE_AUTODIFF ON)
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
  set(MI_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
else()
  set(MI_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

set(MI_COPY_FILES "")

# Function to make a read-only copy of a file
macro (ro_copy IN_FILE OUT_FILE)
  set(OUT_FILE_1 "${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE}")
  set(OUT_FILE_2 "${MI_BINARY_DIR}/${OUT_FILE}")
  if (UNIX)
    add_custom_command(
      OUTPUT ${OUT_FILE_2} DEPENDS ${IN_FILE} ${ARGN}
      COMMAND ${CMAKE_COMMAND} -E copy ${IN_FILE} ${OUT_FILE_2} && chmod a=r ${OUT_FILE_2})
    list(APPEND MI_COPY_FILES ${OUT_FILE_2})
  else()
    # add_custom_command does not support generator expressions in OUTPUT argument (grr..)
    # make a dummy copy and then depend on that
    add_custom_command(
      OUTPUT ${OUT_FILE_1} DEPENDS ${IN_FILE} ${ARGN}
      COMMAND ${CMAKE_COMMAND} -E copy ${IN_FILE} ${OUT_FILE_1}
      COMMAND ${CMAKE_COMMAND} -E copy ${IN_FILE} ${OUT_FILE_2}
    )
    list(APPEND MI_COPY_FILES ${OUT_FILE_1})
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
if ((MI_SANITIZE_ADDRESS OR MI_SANITIZE_MEMORY) AND
    (CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)"))
  # Don't optimize too heavily
  if (U_CMAKE_BUILD_TYPE MATCHES REL)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O1")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -O1")
    add_compile_options(-O1 -fno-optimize-sibling-calls)
  endif()

  add_compile_options(-fno-omit-frame-pointer)

  if (MI_SANITIZE_ADDRESS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    message(STATUS "Mitsuba: enabling the address sanitizer.")
  endif()

  if (MI_SANITIZE_MEMORY)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=memory")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=memory")
    message(STATUS "Mitsuba: enabling the memory sanitizer.")
  endif()
endif()

# Adapt to variations in rpath handling on Linux and macOS
if (APPLE)
  set(MI_ORIGIN "@loader_path")
elseif(UNIX)
  set(MI_ORIGIN "$ORIGIN")
endif()

set(CMAKE_INSTALL_RPATH "${MI_ORIGIN};${MI_ORIGIN}/../drjit")

# Build the dependencies
add_subdirectory(ext)

# Always add the include directories for tinyformat, Dr.Jit and Eigen
include_directories(include
  ${TINYFORMAT_INCLUDE_DIRS}
  ${NANOBIND_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}/include
)

if (MI_ENABLE_CUDA)
  message(STATUS "Mitsuba: using OptiX for GPU ray tracing.")
  add_definitions(-DMI_ENABLE_CUDA=1)
endif()

if (MI_ENABLE_LLVM)
  add_definitions(-DMI_ENABLE_LLVM=1)
endif()

if (MI_ENABLE_EMBREE)
  include_directories(${EMBREE_INCLUDE_DIRS})
  add_definitions(-DMI_ENABLE_EMBREE=1)
  message(STATUS "Mitsuba: using Embree for CPU ray tracing.")
else()
  message(STATUS "Mitsuba: using built-in implementation for CPU ray tracing.")
endif()

if (MI_ENABLE_OPTIX_DEBUG_VALIDATION)
  add_definitions(-DMI_ENABLE_OPTIX_DEBUG_VALIDATION)
  set(DRJIT_ENABLE_OPTIX_DEBUG_VALIDATION ON CACHE BOOL "Dr.Jit: OptiX debug and validation flags enabled." FORCE)
  message(STATUS "Mitsuba: OptiX debug and validation flags enabled.")
else()
  set(DRJIT_ENABLE_OPTIX_DEBUG_VALIDATION OFF CACHE BOOL "Dr.Jit: OptiX debug and validation flags disabled." FORCE)
  message(STATUS "Mitsuba: OptiX debug and validation flags disabled.")
endif()

if (MI_ENABLE_AUTODIFF)
  add_definitions(-DMI_ENABLE_AUTODIFF=1)
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
set(MI_PLUGIN_TARGETS "")
function(add_plugin)
  list(GET ARGV 0 TARGET)
  list(REMOVE_AT ARGV 0)
  add_library(${TARGET} SHARED ${ARGV})
  target_link_libraries(${TARGET} PRIVATE mitsuba)
  set_target_properties(${TARGET} PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${MI_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${MI_BINARY_DIR}/plugins
    FOLDER plugins/${MI_PLUGIN_PREFIX}/${TARGET}
  )
  install(
    TARGETS ${TARGET}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins
  )
  list(APPEND MI_PLUGIN_TARGETS ${TARGET})
  set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
endfunction(add_plugin)

# Be extra noisy about unintended float->double conversions
if (CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)" AND NOT MI_VARIANTS MATCHES "double")
  add_compile_options("-Wdouble-promotion")
endif()

# Forwarding of profiler events to external tools
if (MI_PROFILER_ITTNOTIFY)
  include_directories(${ITT_INCLUDE_DIRS})
  add_definitions(-DMI_ENABLE_ITTNOTIFY=1)
endif()

if (MI_PROFILER_NVTX)
  set(CUDA_TOOLKIT_ROOT_DIR "" CACHE STRING "Path to the CUDA installation root directory.")
  if (NOT CUDA_TOOLKIT_ROOT_DIR)
    message(FATAL_ERROR "Specify CUDA_TOOLKIT_ROOT_DIR.")
  endif()
  include_directories(${CUDA_TOOLKIT_ROOT_DIR}/include)
  add_definitions(-DMI_ENABLE_NVTX=1)
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
        DEPENDS mitsuba-copy-python-src
        USES_TERMINAL)
  endif()
endif()

if (MSVC)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT mitsuba)
endif()

# Warn if a mitsuba.conf file was newly created
if (MI_COPIED_CONFIG_FILE AND NOT SKBUILD)
  message(WARNING "\n${BoldRed}Created a default 'mitsuba.conf' configuration "
          "file. You will probably want to edit this file to specify the "
          "desired configurations before starting to compile.${ColorReset}")
endif()

# Installation targets
set(MI_DEPEND
  IlmImf IlmThread Imath Iex IexMath Half pugixml
)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64)|(amd64)|(AMD64)")
  set(MI_DEPEND ${MI_DEPEND} asmjit)
endif()

list(APPEND MI_DEPEND png jpeg)

if (WIN32)
list(APPEND MI_DEPEND zlib)
endif()

if (MI_ENABLE_EMBREE)
  list(APPEND MI_DEPEND embree)
endif()

if (NOT SKBUILD)
  list(APPEND MI_DEPEND nanothread)

  if (MI_ENABLE_JIT)
    list(APPEND MI_DEPEND drjit-core)
  endif()

  if (MI_ENABLE_AUTODIFF)
    list(APPEND MI_DEPEND drjit-extra)
  endif()
endif()

# Copy shared libraries to Python folders (no rpath on Windows)
if (MSVC AND MI_ENABLE_PYTHON)
  set(COPY_TARGETS mitsuba ${MI_DEPEND} ${MI_PLUGIN_TARGETS})
  add_custom_target(copy-targets ALL DEPENDS ${COPY_TARGETS})

  foreach(target ${COPY_TARGETS})
    get_target_property(TARGET_FOLDER ${target} FOLDER)
    if(TARGET_FOLDER MATCHES "plugins/.*$")
        set(COPY_TARGET_DESTINATION ${MI_BINARY_DIR}/python/mitsuba/plugins/$<TARGET_FILE_NAME:${target}>)
    else()
        set(COPY_TARGET_DESTINATION ${MI_BINARY_DIR}/python/mitsuba/$<TARGET_FILE_NAME:${target}>)
    endif()
    add_custom_command(
      TARGET copy-targets POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${target}> ${COPY_TARGET_DESTINATION}
      DEPENDS ${target}
    )
  endforeach(target)

  add_dependencies(copy-targets-python copy-targets)
endif()

# Set up location for build products
set_target_properties(mitsuba-bin mitsuba ${MI_DEPEND}
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${MI_BINARY_DIR}
  LIBRARY_OUTPUT_DIRECTORY ${MI_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${MI_BINARY_DIR}
  LIBRARY_OUTPUT_DIRECTORY_RELEASE ${MI_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${MI_BINARY_DIR}
  LIBRARY_OUTPUT_DIRECTORY_DEBUG ${MI_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_RELNODEBINFO ${MI_BINARY_DIR}
  LIBRARY_OUTPUT_DIRECTORY_RELNODEBINFO ${MI_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${MI_BINARY_DIR}
  LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${MI_BINARY_DIR}
)

install(
  TARGETS mitsuba-bin mitsuba ${MI_DEPEND}
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

set(MI_SRGB_COEFF_FILE "" CACHE STRING "Location of the spectral upsampling model file (srgb.coeff) if it should not be generated during the build.")
mark_as_advanced(MI_SRGB_COEFF_FILE)
if ("${MI_SRGB_COEFF_FILE}" STREQUAL "")
  ro_copy(${CMAKE_CURRENT_BINARY_DIR}/ext/rgb2spec/srgb.coeff data/srgb.coeff rgb2spec_opt_run)
  if (MSVC AND MI_ENABLE_PYTHON)
    ro_copy(${CMAKE_CURRENT_BINARY_DIR}/ext/rgb2spec/srgb.coeff python/mitsuba/data/srgb.coeff rgb2spec_opt_run)
  endif()
else()
  ro_copy(${MI_SRGB_COEFF_FILE} data/srgb.coeff)
  if (MSVC AND MI_ENABLE_PYTHON)
    ro_copy(${MI_SRGB_COEFF_FILE} python/mitsuba/data/srgb.coeff)
  endif()
endif()
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/data/srgb.coeff
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}
)

add_custom_target(copy-resources ALL DEPENDS ${MI_COPY_FILES})

file(GLOB IOR_FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/data/ior/*spd")
add_custom_command(
  TARGET copy-resources DEPENDS IOR_FILES
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${CMAKE_CURRENT_SOURCE_DIR}/resources/data/ior"
  "${MI_BINARY_DIR}/data/ior"
)
if (MSVC AND MI_ENABLE_PYTHON)
  add_custom_command(
    TARGET copy-resources DEPENDS IOR_FILES
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/data/ior"
    "${MI_BINARY_DIR}/python/mitsuba/data/ior"
  )
endif()
install(
  DIRECTORY ${MI_BINARY_DIR}/data/ior
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}
)
