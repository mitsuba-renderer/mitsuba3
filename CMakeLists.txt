# This is the main CMake build file used to compile Mitsuba
cmake_minimum_required (VERSION 2.8)
project(mitsuba)

option(MTS_DOUBLE_PRECISION "Compile renderer in double precision?" FALSE)

cmake_policy(SET CMP0022 NEW) # New-style link interface
cmake_policy(SET CMP0056 NEW) # try_compile: pass linker flags to compiler

include(CheckCXXCompilerFlag)
include(TestBigEndian)

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
    ARCHFLAGS_FLAGS ARCHFLAGS_COMPILE_RESULT
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/archflags.cpp
  )
  if(NOT ${ARCHFLAGS_COMPILE_RESULT})
     message(FATAL_ERROR "Failed to compile 'archflags' binary")
  endif()
  add_compile_options(${ARCHFLAGS_FLAGS})
endif()

# Disable overly aggressive FP optimization in the Intel compiler
# (this flag is intentially added here so that it also applies to dependencies)
if (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  add_compile_options("-fp-model" "precise")
endif()

# Always use libc++ on Clang
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
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

# Always add the include directories for tinyformat, SIMDArray and Eigen
include_directories(include
  ${TINYFORMAT_INCLUDE_DIRS}
  ${EIGEN_INCLUDE_DIRS}
  ${SIMDARRAY_INCLUDE_DIRS}
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
  else()
    # Be extra noisy about unintended float->double conversions
    add_compile_options("-Wdouble-promotion")
  endif()
endif()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang|Intel)$")
  # Enable C++11/14
  CHECK_CXX_COMPILER_FLAG("-std=c++14" HAS_CPP14_FLAG)

  if (HAS_CPP14_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
  else()
    message(FATAL_ERROR "Unsupported compiler -- Mitsuba requires C++14 support!")
  endif()

  # Enable link time optimization and set the default symbol
  # visibility to hidden (very important to obtain small binaries)
  if (U_CMAKE_BUILD_TYPE MATCHES REL)
    # Don't use stack security features in release mode
    add_compile_options("-fno-stack-protector")

    # In release mode, don't keep the frame pointer in a special register unless needed
    add_compile_options("-fomit-frame-pointer")

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
  endif()

  # Set the default symbol visibility to hidden
  if (U_CMAKE_BUILD_TYPE MATCHES RELEASE OR
      U_CMAKE_BUILD_TYPE MATCHES MINSIZEREL)
    add_compile_options("-fvisibility=hidden")
  endif()
endif()

if (MSVC)
  # Disable annoying MSVC warnings (all targets)
  add_definitions(/D "_CRT_SECURE_NO_WARNINGS")

  # Don't issue implicit linking pragmas for TBB
  add_definitions(/D "__TBB_NO_IMPLICIT_LINKAGE")

  # Don't complain about not DLL-exporting STL classes
  add_compile_options("/wd4251")

  # Parallel build on MSVC
  add_compile_options("/MP")

  # Permit many sections in .obj files
  add_compile_options("/bigobj")

  # Disable buffer security check cookie
  string(REPLACE "/GS" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REPLACE "/GS" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  add_compile_options("$<$<NOT:$<CONFIG:Debug>>:/GS->")

  if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    # Enable SSE2 instructions on 32-bit builds builds
    add_compile_options("/arch:SSE2")

    # Disable Eigen vectorization for Windows 32-bit builds (issues with unaligned access segfaults)
    add_definitions(/D "EIGEN_DONT_ALIGN")
  endif()
endif()

# Enable link time optimizatoin in release mode (if possible)
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
      message(STATUS CMAKE_${LinkType}_LINKER_FLAGS_${Configuration})
    endforeach()
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

if (MTS_DOUBLE_PRECISION)
  add_definitions(-DDOUBLE_PRECISION)
else()
  add_definitions(-DSINGLE_PRECISION)
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
function(add_plugin)
  list(GET ARGV 0 target_name)
  list(REMOVE_AT ARGV 0)
  add_library(${target_name}-obj OBJECT ${ARGV})
  add_library(${target_name} SHARED $<TARGET_OBJECTS:${target_name}-obj>)
  set_property(TARGET ${target_name} PROPERTY POSITION_INDEPENDENT_CODE ON)
  set_property(TARGET ${target_name}-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
  set_target_properties(${target_name} PROPERTIES PREFIX "")
  target_link_libraries(${target_name} PRIVATE mitsuba-core mitsuba-render)
  add_custom_command(
    TARGET ${target_name}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target_name}> ${CMAKE_BINARY_DIR}/dist/plugins)
endfunction(add_plugin)

# For the Mitsuba plugins and libraries, don't set a build/install rpath
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "")

enable_testing()

# Register the Mitsuba codebase
add_subdirectory(src)
