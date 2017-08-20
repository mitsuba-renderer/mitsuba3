# This is the main CMake build file used to compile Mitsuba
cmake_minimum_required (VERSION 2.8.12)
project(mitsuba CXX)

option(MTS_DOUBLE_PRECISION  "Compile renderer in double precision?" OFF)

# Use GCC/Clang address sanitizer?
# NOTE: To use this in conjunction with Python plugin, you will need to call
# On OSX:
#   export DYLD_INSERT_LIBRARIES=<path to libclang_rt.asan_osx_dynamic.dylib>
# On Linux:
#   export LD_LIBRARY_PATH=<path to libasan.so>

option(MTS_ADDRESS_SANITIZER "Enable GCC/Clang address sanitizer?" OFF)

cmake_policy(SET CMP0022 NEW) # New-style link interface
cmake_policy(SET CMP0056 NEW) # try_compile: pass linker flags to compiler

include(CheckCXXCompilerFlag)
include(TestBigEndian)
include(CheckCXXSourceRuns)

# Set a default build configuration (Release)
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

# Optimize for current architecture
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
    CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  add_compile_options("-march=native")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  add_compile_options("-xHost")
else(MSVC)
  # MSVC doesn't have a flag equivalent to -march=native
  # The 'archflags' executable provides this functionality
  try_run(
    ARCHFLAGS_SUCCESS
    ARCHFLAGS_COMPILE_RESULT
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/archflags.cpp
    RUN_OUTPUT_VARIABLE ARCHFLAGS_FLAGS
  )
  if(NOT ${ARCHFLAGS_COMPILE_RESULT})
     message(FATAL_ERROR "Failed to compile 'archflags' binary")
  endif()
  add_compile_options(${ARCHFLAGS_FLAGS})
  message(STATUS "Mitsuba: Compiling with ${ARCHFLAGS_FLAGS}")
endif()

# Disable overly aggressive FP optimization in the Intel compiler
# (this flag is intentially added here so that it also applies to dependencies)
if (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  add_compile_options("-fp-model" "precise")
endif()

macro(CHECK_CXX_COMPILER_AND_LINKER_FLAGS _RESULT _CXX_FLAGS _LINKER_FLAGS)
  set(CMAKE_REQUIRED_FLAGS ${_CXX_FLAGS})
  set(CMAKE_REQUIRED_LIBRARIES ${_LINKER_FLAGS})
  set(CMAKE_REQUIRED_QUIET TRUE)
  check_cxx_source_runs("#include <iostream> int main(int argc, char **argv) { return 0; }" ${_RESULT})
  set(CMAKE_REQUIRED_FLAGS "")
  set(CMAKE_REQUIRED_LIBRARIES "")
endmacro()

# Prefer libc++ in conjunction with Clang
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Always use libc++ on Clang
  CHECK_CXX_COMPILER_AND_LINKER_FLAGS(HAS_LIBCPP "-stdlib=libc++" "-stdlib=libc++")
  if (HAS_LIBCPP)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
    message(STATUS "Enoki: using libc++.")
  else()
    message(STATUS "Enoki: NOT using libc++.")
  endif()
endif()

# Enable folders for projects in Visual Studio
if (CMAKE_GENERATOR MATCHES "Visual Studio")
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

# Enable default rpaths
if (APPLE)
  set(CMAKE_MACOSX_RPATH ON)
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

# Compile with compiler warnings turned on
if (MSVC)
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    add_compile_options("/W4")
  endif()
else()
  add_compile_options("-Wall" "-Wextra")
  if (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    # Quench inlining-related remarks
    add_compile_options("-wd11074" "-wd11076")
  endif()
  if (NOT MTS_DOUBLE_PRECISION)
    # Be extra noisy about unintended float->double conversions
    add_compile_options("-Wdouble-promotion")
  endif()
endif()

# Language and optimization flags that are used to compile Mitsuba
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|Intel")
  # Enable C++14
  CHECK_CXX_COMPILER_FLAG("-std=c++14" HAS_CPP14_FLAG)

  if (HAS_CPP14_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
  else()
    message(FATAL_ERROR "Unsupported compiler -- Mitsuba requires C++14 support!")
  endif()

  # Enable link time optimization and set the default symbol
  # visibility to hidden (very important to obtain small binaries)
  if (U_CMAKE_BUILD_TYPE MATCHES REL)
    if (NOT MTS_ADDRESS_SANITIZER)
      # Don't use stack security features in release mode
      add_compile_options("-fno-stack-protector")

      # In release mode, don't keep the frame pointer in a special register unless needed
      add_compile_options("-fomit-frame-pointer")
    endif()

    # Don't update the 'errno' global when encountering domain errors in math functions
    add_compile_options("-fno-math-errno")

    # Increase function size limits for inlining on GCC (useful for templated
    # code where significant portions can be optimized away during inlining)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
      add_compile_options("-finline-limit=150")
    endif()

    # Contract multiplications and additions into fused multiply-add instructions
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Intel")
      add_compile_options("-ffp-contract=fast")
    endif()

    # Set the default symbol visibility to hidden
    add_compile_options("-fvisibility=hidden")
  endif()
endif()

if (MSVC)
  # Disable annoying MSVC warnings (all targets)
  add_definitions(/D "_CRT_SECURE_NO_WARNINGS" /D "_SCL_SECURE_NO_WARNINGS")

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

  # Workaround for MSVC 2017
  add_compile_options("/wd4244")

  # Parallel build on MSVC
  add_compile_options("/MP")

  # Permit many sections in .obj files
  add_compile_options("/bigobj")

  # Disable buffer security check cookie
  string(REPLACE "/GS" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REPLACE "/GS" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  add_compile_options("$<$<NOT:$<CONFIG:Debug>>:/GS->")

  # Honor __forceinline statements even in debug mode, needed to avoid internal compiler errors on MSVC
  string(REPLACE "/Ob0" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  add_compile_options("$<$<CONFIG:Debug>:/Ob1>")

   # Enable intrinsic functions
   add_compile_options("$<$<CONFIG:Release>:/Oi>")
endif()

# Enable link time optimization in release mode (if possible)
if (U_CMAKE_BUILD_TYPE MATCHES REL AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
  # Enable link time optimization
  if (NOT CMAKE_CXX_FLAGS MATCHES "-flto")
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
    set(HAS_LTO OFF)

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
    set("CMAKE_CXX_FLAGS_${Configuration}" "${CMAKE_CXX_FLAGS_${Configuration}} /GL")
    foreach(LinkType ${LinkTypes})
      set("CMAKE_${LinkType}_LINKER_FLAGS_${Configuration}" "${CMAKE_${LinkType}_LINKER_FLAGS_${Configuration}} /LTCG")
    endforeach()
  endforeach()
  message(STATUS "Mitsuba: LTO support enabled.")
endif()

# Clang/GCC Memory sanitizer
if (MTS_ADDRESS_SANITIZER AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
  # Don't optimize too heavily
  if (U_CMAKE_BUILD_TYPE MATCHES REL)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O1")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -O1")
    add_compile_options("-O1")
  endif()

  add_compile_options("-fno-omit-frame-pointer")
  add_compile_options("-fsanitize=address")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
endif()

# Use memkind library (high-bandwidth memory on Xeon Phi) if available
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/ext/enoki/resources")
find_package(Memkind QUIET)

if (MEMKIND_FOUND)
  message(STATUS "Mitsuba: Using memkind library for Enoki heap memory allocation.")
  include_directories(${MEMKIND_INCLUDE_DIRS})
  set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} ${MEMKIND_LIBRARIES}")
  add_definitions(-DENOKI_USE_MEMKIND)
else()
  message(STATUS "Mitsuba: Using C library for Enoki heap memory allocation.")
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

if (MTS_DOUBLE_PRECISION)
  add_definitions(-DDOUBLE_PRECISION)
  message(STATUS "Mitsuba: Compiling using double precision.")
else()
  add_definitions(-DSINGLE_PRECISION)
  message(STATUS "Mitsuba: Compiling using single precision.")
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
  set(MITSUBA_TEST_DIRECTORIES ${MITSUBA_TEST_DIRECTORIES} ${ARGV} CACHE INTERNAL "")
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
  target_link_libraries(${TARGET} PRIVATE mitsuba-core mitsuba-render)
  add_dist(plugins/${TARGET})
  set_target_properties(${TARGET} ${TARGET}-obj PROPERTIES FOLDER plugins/${MTS_PLUGIN_PREFIX}/${TARGET})
endfunction(add_plugin)

# Initialize variables
set(MITSUBA_DIST "" CACHE INTERNAL "")
set(MITSUBA_TEST_DIRECTORIES "" CACHE INTERNAL "")

# For the Mitsuba plugins and libraries, don't set a build/install rpath
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "")

# Register the Mitsuba codebase
add_subdirectory(src)

# Copy dependencies into the 'dist' directory
cmake_policy(SET CMP0026 OLD)
foreach(ITEM ${MITSUBA_DIST})
  get_filename_component(TARGET_NAME ${ITEM} NAME)
  get_filename_component(TARGET_DIRECTORY ${ITEM} DIRECTORY)
  get_target_property(TARGET_FILENAME ${TARGET_NAME} LOCATION)
  get_filename_component(TARGET_FILENAME ${TARGET_FILENAME} NAME)
  if (TARGET_DIRECTORY STREQUAL "")
    set(TARGET_DIRECTORY ".")
  endif()
  set(OUTPUT_FILE ${CMAKE_BINARY_DIR}/dist/${TARGET_DIRECTORY}/${TARGET_FILENAME})
  add_custom_command(
    OUTPUT ${OUTPUT_FILE} DEPENDS ${TARGET_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET_NAME}> ${OUTPUT_FILE})
  list(APPEND MITSUBA_DIST_OUT ${OUTPUT_FILE})
endforeach()

add_custom_target(dist-copy ALL DEPENDS ${MITSUBA_DIST_OUT})
set_property(TARGET dist-copy PROPERTY FOLDER misc)
