# ---------------------------------------------------------------------------
#  Undo 'cmake-defaults' settings that interfere with dependencies
# ---------------------------------------------------------------------------

get_directory_property(MI_EXT_COMPILE_OPTIONS        COMPILE_OPTIONS)
get_directory_property(MI_EXT_COMPILE_OPTIONS_NOWARN COMPILE_OPTIONS)
list(REMOVE_ITEM MI_EXT_COMPILE_OPTIONS_NOWARN -Wall -Wextra /W4)

# ---------------------------------------------------------------------------
#  Link against Intel ITT (profiler events for VTune, optional)
# ---------------------------------------------------------------------------

if (MI_PROFILER_ITTNOTIFY)
  set_directory_properties(PROPERTIES COMPILE_OPTIONS "${MI_EXT_COMPILE_OPTIONS_NOWARN}")
  add_subdirectory(ittnotify/ittnotify)
  set(ITT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/ittnotify/ittnotify/include)
  set(ITT_INCLUDE_DIRS ${ITT_INCLUDE_DIRS} PARENT_SCOPE)
  set_property(TARGET ittnotify PROPERTY FOLDER "dependencies")
  message(STATUS "Mitsuba: emitting profiling events with Intel ITT.")
endif()

# ---------------------------------------------------------------------------
#  Build Dr.Jit (+ Python bindings, JIT compiler, etc.)
# ---------------------------------------------------------------------------

set_directory_properties(PROPERTIES COMPILE_OPTIONS "${MI_EXT_COMPILE_OPTIONS}")
set(DRJIT_ENABLE_JIT      ${MI_ENABLE_JIT} CACHE BOOL " " FORCE)
set(DRJIT_ENABLE_AUTODIFF ${MI_ENABLE_AUTODIFF} CACHE BOOL " " FORCE)
set(DRJIT_ENABLE_PYTHON   ${MI_ENABLE_PYTHON} CACHE BOOL " " FORCE)
if (MI_PROFILER_NVTX)
    include_directories(${CUDA_TOOLKIT_ROOT_DIR}/include)
    set(DRJIT_ENABLE_NVTX ON)
endif()

if (NOT SKBUILD)
  if (MSVC)
    set(DRJIT_PYTHON_DST_DIR "${MI_BINARY_DIR}/python/drjit")
  else()
    set(DRJIT_PYTHON_DST_DIR "${CMAKE_CURRENT_BINARY_DIR}/../python/drjit")
  endif()
  add_subdirectory(drjit)

  if (NOT MI_ENABLE_JIT)
    add_subdirectory(drjit/ext/drjit-core/ext/nanothread)
    set_target_properties(nanothread PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/drjit")
  endif()

  set_target_properties(nanothread PROPERTIES FOLDER "drjit")

  if (MI_ENABLE_JIT)
    set_target_properties(drjit-core PROPERTIES FOLDER "drjit")
  endif()

  if (MI_ENABLE_AUTODIFF)
    set_target_properties(drjit-extra PROPERTIES FOLDER "drjit")
  endif()

  if (MI_ENABLE_PYTHON)
    set_target_properties(drjit-python PROPERTIES FOLDER "drjit")
  endif()
endif()

mark_as_advanced(
  DRJIT_ENABLE_AUTODIFF DRJIT_ENABLE_JIT
  DRJIT_ENABLE_PYTHON DRJIT_ENABLE_PYTHON_PACKET DRJIT_ENABLE_TESTS
  DRJIT_JIT_DYNAMIC_CUDA DRJIT_JIT_DYNAMIC_LLVM DRJIT_JIT_ENABLE_OPTIX
  DRJIT_THREAD_ENABLE_TESTS DRJIT_ENABLE_OPTIX_DEBUG_VALIDATION
)

# ----------------------------------------------------------
#  Compile Intel Embree (optional)
# ----------------------------------------------------------

if (MI_ENABLE_EMBREE)
  set_directory_properties(PROPERTIES COMPILE_OPTIONS "")

  set(EMBREE_ISPC_SUPPORT              OFF CACHE BOOL " " FORCE)
  set(EMBREE_TUTORIALS                 OFF CACHE BOOL " " FORCE)
  set(EMBREE_FILTER_FUNCTION           OFF CACHE BOOL " " FORCE)
  set(EMBREE_IGNORE_CMAKE_CXX_FLAGS    OFF CACHE BOOL " " FORCE)
  set(EMBREE_GEOMETRY_QUAD             OFF CACHE BOOL " " FORCE)
  set(EMBREE_GEOMETRY_GRID             OFF CACHE BOOL " " FORCE)
  set(EMBREE_GEOMETRY_POINT            OFF CACHE BOOL " " FORCE)
  set(EMBREE_GEOMETRY_SUBDIVISION      OFF CACHE BOOL " " FORCE)
  set(EMBREE_GEOMETRY_CURVE            ON  CACHE BOOL " " FORCE)
  set(EMBREE_BACKFACE_CULLING_CURVES   ON  CACHE BOOL " " FORCE)
  set(EMBREE_GEOMETRY_INSTANCE         ON  CACHE BOOL " " FORCE)
  set(EMBREE_GEOMETRY_USER             ON  CACHE BOOL " " FORCE)
  set(EMBREE_IGNORE_INVALID_RAYS       ON  CACHE BOOL " " FORCE)
  set(EMBREE_RAY_MASK                  OFF CACHE BOOL " " FORCE)
  set(EMBREE_MAX_ISA "NONE"            CACHE STRING " " FORCE)
  set(EMBREE_STAT_COUNTERS             OFF CACHE BOOL " " FORCE)
  set(EMBREE_MAX_INSTANCE_LEVEL_COUNT  1 CACHE STRING " " FORCE)
  set(EMBREE_TASKING_SYSTEM            "INTERNAL" CACHE STRING " " FORCE)
  set(EMBREE_ISA_SSE2                  OFF CACHE BOOL " " FORCE)

  if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(EMBREE_ISA_SSE42            OFF CACHE BOOL " " FORCE)
    set(EMBREE_ISA_AVX              OFF CACHE BOOL " " FORCE)
    set(EMBREE_ISA_AVX2             OFF CACHE BOOL " " FORCE)
    set(EMBREE_ISA_AVX512           OFF CACHE BOOL " " FORCE)
    set(EMBREE_ISA_NEON             ON CACHE BOOL " " FORCE)
    set(EMBREE_ISA_NEON2X           ON CACHE BOOL " " FORCE)
  else()
    set(EMBREE_ISA_SSE42            ON CACHE BOOL " " FORCE)
    set(EMBREE_ISA_AVX              ON CACHE BOOL " " FORCE)
    set(EMBREE_ISA_AVX2             ON CACHE BOOL " " FORCE)
    if (MSVC)
        set(EMBREE_ISA_AVX512SKX    OFF CACHE BOOL " " FORCE)
    else()
        set(EMBREE_ISA_AVX512SKX    ON CACHE BOOL " " FORCE)
    endif()
    set(EMBREE_ISA_NEON             OFF CACHE BOOL " " FORCE)
    set(EMBREE_ISA_NEON2X           OFF CACHE BOOL " " FORCE)
  endif()

  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-Wno-unused-variable)
  endif()

  # Briefly remove -march=native and let Embree do it's own ISA selection
  unset(CMAKE_CXX_VISIBILITY_PRESET)
  add_subdirectory(embree)
  set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
  set(EMBREE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/embree/include" PARENT_SCOPE)

  if (MSVC)
    # Don't complain about ignored alignment specifier
    target_compile_options(embree PRIVATE "/wd4359")
  endif()

  set_property(TARGET
    lexers math simd sys tasking embree ${EMBREE_TARGET}
    PROPERTY FOLDER "dependencies/embree")
endif()

mark_as_advanced(FORCE EMBREE_ADDRESS_SANITIZER EMBREE_API_NAMESPACE
  EMBREE_BACKFACE_CULLING EMBREE_COMPACT_POLYS
  EMBREE_CURVE_SELF_INTERSECTION_AVOIDANCE_FACTOR EMBREE_FILTER_FUNCTION
  EMBREE_GEOMETRY_CURVE EMBREE_GEOMETRY_GRID EMBREE_GEOMETRY_INSTANCE
  EMBREE_GEOMETRY_POINT EMBREE_GEOMETRY_QUAD EMBREE_GEOMETRY_SUBDIVISION
  EMBREE_GEOMETRY_TRIANGLE EMBREE_GEOMETRY_USER EMBREE_IGNORE_CMAKE_CXX_FLAGS
  EMBREE_IGNORE_INVALID_RAYS EMBREE_ISA_AVX EMBREE_ISA_AVX2 EMBREE_ISA_AVX512
  EMBREE_ISA_AVX512SKX EMBREE_ISA_NEON EMBREE_ISA_NEON2X EMBREE_ISA_SSE2
  EMBREE_ISA_SSE42 EMBREE_ISPC_SUPPORT EMBREE_LIBRARY_NAME
  EMBREE_MAX_INSTANCE_LEVEL_COUNT EMBREE_MAX_ISA EMBREE_MIN_WIDTH
  EMBREE_RAY_MASK EMBREE_RAY_PACKETS EMBREE_STACK_PROTECTOR EMBREE_STATIC_LIB
  EMBREE_STAT_COUNTERS EMBREE_TASKING_SYSTEM EMBREE_TBB_COMPONENT
  EMBREE_TESTING_BENCHMARK EMBREE_TESTING_BENCHMARK_DATABASE
  EMBREE_TESTING_INTENSITY EMBREE_TESTING_KLOCWORK EMBREE_TESTING_MEMCHECK
  EMBREE_TESTING_MODEL_DIR EMBREE_TESTING_PACKAGE EMBREE_TESTING_SDE
  EMBREE_TUTORIALS EMBRE_STATIC_LIB EMBREE_STATIC_RUNTIME)

# ----------------------------------------------------------
#  Build OpenEXR (and zlib on Windows)
# ----------------------------------------------------------

set_directory_properties(PROPERTIES COMPILE_OPTIONS "${MI_EXT_COMPILE_OPTIONS_NOWARN}")
if (WIN32)
  set(ZLIB_BUILD_STATIC_LIBS OFF CACHE BOOL " " FORCE)
  set(ZLIB_BUILD_SHARED_LIBS ON  CACHE BOOL " " FORCE)
  add_subdirectory(zlib)

  set_property(TARGET zlib PROPERTY FOLDER "dependencies")
  set(ZLIB_LIBRARY zlib)
  set(ZLIB_INCLUDE_DIR
      ${CMAKE_CURRENT_SOURCE_DIR}/zlib
      ${CMAKE_CURRENT_BINARY_DIR}/zlib
      CACHE BOOL " " FORCE)

  mark_as_advanced(ZLIB_BUILD_AMD64 ZLIB_BUILD_ASM686 ZLIB_BUILD_EXAMPLES
    ZLIB_BUILD_SHARED_LIBS ZLIB_BUILD_STATIC_LIBS)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  add_compile_options(-Wno-unused-parameter -Wno-unused-but-set-variable -Wno-sign-compare)
endif()

# Prevents openexr to set the DEBUG_POSTFIX "_d"
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING " " FORCE)
set(ILMBASE_LIB_SUFFIX  "" CACHE STRING "" FORCE)
set(OPENEXR_LIB_SUFFIX  "" CACHE STRING "" FORCE)
set(OPENEXR_BUILD_UTILS        OFF CACHE BOOL "" FORCE)
set(ILMBASE_INSTALL_PKG_CONFIG OFF CACHE BOOL "" FORCE)
set(OPENEXR_INSTALL_PKG_CONFIG OFF CACHE BOOL "" FORCE)
set(PYILMBASE_ENABLE           OFF CACHE BOOL "" FORCE)
set(INSTALL_OPENEXR_DOCS OFF CACHE BOOL "" FORCE)
set(INSTALL_OPENEXR_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

unset(CMAKE_CXX_VISIBILITY_PRESET)
add_subdirectory(openexr)
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")

if (MSVC)
  target_compile_options(Half PRIVATE /W2)
  target_compile_options(IlmThread PRIVATE /W2)
  target_compile_options(Imath PRIVATE /W2)
  target_compile_options(IlmImf PRIVATE /W1)
endif()

set_property(TARGET
  IexMath IlmThread Half Iex Imath IlmImf IexMath IlmImfUtil
  PROPERTY FOLDER "dependencies/openexr")

set_property(TARGET
  IexMath IlmThread Half Iex Imath IlmImf IexMath
  PROPERTY PUBLIC_HEADER "")

foreach(X IexMath IlmThread Half Iex Imath IlmImf IexMath IlmImfUtil)
  set_property(TARGET ${X} PROPERTY OUTPUT_NAME "${X}-mitsuba")
endforeach()

set(OPENEXR_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/openexr/IlmBase/Imath
  ${CMAKE_CURRENT_SOURCE_DIR}/openexr/IlmBase/Imath
  ${CMAKE_CURRENT_SOURCE_DIR}/openexr/IlmBase/Imath
  ${CMAKE_CURRENT_SOURCE_DIR}/openexr/IlmBase/Iex
  ${CMAKE_CURRENT_SOURCE_DIR}/openexr/IlmBase/Half
  ${CMAKE_CURRENT_SOURCE_DIR}/openexr/OpenEXR/IlmImf
  ${CMAKE_CURRENT_BINARY_DIR}/openexr/OpenEXR/config
  ${CMAKE_CURRENT_BINARY_DIR}/openexr/IlmBase/config
  PARENT_SCOPE)

mark_as_advanced(
  ILMBASE_BUILD_BOTH_STATIC_SHARED ILMBASE_ENABLE_LARGE_STACK
  ILMBASE_IEX_NAMESPACE ILMBASE_ILMTHREAD_NAMESPACE ILMBASE_IMATH_NAMESPACE
  ILMBASE_INSTALL_PKG_CONFIG ILMBASE_INTERNAL_IEX_NAMESPACE
  ILMBASE_INTERNAL_ILMTHREAD_NAMESPACE ILMBASE_INTERNAL_IMATH_NAMESPACE
  ILMBASE_LIB_SUFFIX ILMBASE_NAMESPACE_CUSTOM ILMBASE_OUTPUT_SUBDIR
  ILMBASE_PACKAGE_NAME ILMBASE_STATIC_LIB_SUFFIX IlmBase_DIR PYILMBASE_ENABLE
)

mark_as_advanced(
  OPENEXR_BUILD_BOTH_STATIC_SHARED OPENEXR_BUILD_UTILS OPENEXR_CXX_STANDARD
  OPENEXR_IMF_NAMESPACE OPENEXR_INSTALL_PKG_CONFIG OPENEXR_INTERNAL_IMF_NAMESPACE
  OPENEXR_LIB_SUFFIX OPENEXR_NAMESPACE_CUSTOM OPENEXR_OUTPUT_SUBDIR
  OPENEXR_PACKAGE_NAME OPENEXR_RUN_FUZZ_TESTS OPENEXR_STATIC_LIB_SUFFIX
  OPENEXR_USE_CLANG_TIDY OpenEXR_DIR INSTALL_OPENEXR_DOCS
  INSTALL_OPENEXR_EXAMPLES
)

# ----------------------------------------------------------
#  libpng
# ----------------------------------------------------------

set(PNG_SHARED ON CACHE BOOL " " FORCE)
set(PNG_STATIC OFF CACHE BOOL " " FORCE)
set(PNG_TESTS OFF CACHE BOOL " " FORCE)
set(PNG_SKIP_INSTALL_ALL TRUE)
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
set(PNG_ARM_NEON "on" CACHE STRING " " FORCE)
endif()
add_subdirectory(libpng)
set_property(TARGET png genfiles PROPERTY FOLDER "dependencies")
set_property(TARGET png PROPERTY OUTPUT_NAME "png-mitsuba")

set(PNG_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/libpng;${CMAKE_CURRENT_BINARY_DIR}/libpng" PARENT_SCOPE)
set(PNG_LIBRARIES    "png" PARENT_SCOPE)
set(PNG_DEFINES      -DMI_HAS_LIBPNG PARENT_SCOPE)

mark_as_advanced(DFA_XTRA AWK PNG_FRAMEWORK PNG_HARDWARE_OPTIMIZATIONS
  PNG_PREFIX PNG_SHARED PNG_STATIC PNG_TESTS PNG_BUILD_ZLIB PNG_DEBUG
  PNG_INTEL_SSE M_LIBRARY ld-version-script)

# ----------------------------------------------------------
#  libjpeg 7
# ----------------------------------------------------------

set(LIBJPEG_BUILD_SHARED ON CACHE BOOL " " FORCE)
set(LIBJPEG_BUILD_EXECUTABLES OFF CACHE BOOL " " FORCE)
add_subdirectory(libjpeg)
set_property(TARGET jpeg PROPERTY FOLDER "dependencies")
set(JPEG_LIBRARIES libjpeg PARENT_SCOPE)

set(JPEG_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/libjpeg;${CMAKE_CURRENT_BINARY_DIR}/libjpeg" PARENT_SCOPE)
set(JPEG_LIBRARIES    "jpeg" PARENT_SCOPE)
set(JPEG_DEFINES      -DMI_HAS_LIBJPEG PARENT_SCOPE)

# Give libpng & libjpeg a name that's guaranteeed not to match other
# libraries that may already be loaded (e.g. into a Python interpreter)
set_property(TARGET jpeg PROPERTY OUTPUT_NAME "jpeg-mitsuba")

mark_as_advanced(
  LIBJPEG_BUILD_EXECUTABLES
  LIBJPEG_BUILD_SHARED
)

# ----------------------------------------------------------
#  pugixml XML parser
# ----------------------------------------------------------

add_library(pugixml SHARED pugixml/src/pugixml.cpp)
set_property(TARGET pugixml PROPERTY
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pugixml")
set_property(TARGET pugixml PROPERTY FOLDER "dependencies")
set(PUGIXML_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/pugixml/src PARENT_SCOPE)
target_compile_options(pugixml PRIVATE -DPUGIXML_BUILD_DLL)

# tinyformat include path
set(TINYFORMAT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/tinyformat PARENT_SCOPE)

# ----------------------------------------------------------
#  asmjit -- x86/64 JIT compiler
# ----------------------------------------------------------

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64|AMD64")
  # Build asmjit
  set(ASMJIT_NO_AARCH32    TRUE CACHE BOOL "" FORCE)
  set(ASMJIT_NO_AARCH64    TRUE CACHE BOOL "" FORCE)
  set(ASMJIT_NO_DEPRECATED TRUE CACHE BOOL "" FORCE)

  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-inconsistent-missing-override -Wno-undefined-inline")
  endif()
  add_subdirectory(asmjit)
  set(ASMJIT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/asmjit/src PARENT_SCOPE)
  set_property(TARGET asmjit PROPERTY FOLDER "dependencies")
  if (MSVC)
    target_compile_options(asmjit PRIVATE "/wd4804" "/wd4838")
    # Don't complain about ignoring '/INCREMENTAL'
    set_target_properties(asmjit PROPERTIES LINK_FLAGS "/ignore:4075")
  endif()
  # Avoid name clashes with pytorch (which also includes asmjit)
  set_property(TARGET asmjit PROPERTY OUTPUT_NAME "asmjit-mitsuba")
endif()

mark_as_advanced(
  ASMJIT_DIR ASMJIT_EMBED ASMJIT_STATIC ASMJIT_NO_AARCH32 ASMJIT_NO_AARCH64
  ASMJIT_NO_DEPRECATED ASMJIT_NO_CUSTOM_FLAGS ASMJIT_NO_FOREIGN ASMJIT_NO_NATVIS
  ASMJIT_NO_X86 ASMJIT_TEST ASMJIT_SANITIZE
)

# ----------------------------------------------------------
#  sRGB spectral upsampling model
# ----------------------------------------------------------

# build rgb2spec
add_subdirectory(rgb2spec)
set(RGB2SPEC_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/rgb2spec PARENT_SCOPE)
set_target_properties(rgb2spec rgb2spec_opt rgb2spec_opt_run PROPERTIES FOLDER "dependencies/rgb2spec")

if (MSVC)
  target_compile_options(rgb2spec_opt PRIVATE "/wd4849" "/wd4456")
endif()

# ----------------------------------------------------------
#  Fast floating point parser (interface library)
# ----------------------------------------------------------

add_subdirectory(fastfloat EXCLUDE_FROM_ALL)
mark_as_advanced(
  FASTFLOAT_SANITIZE
  FASTFLOAT_TEST
)
unset(CMAKE_CXX_STANDARD CACHE)

# ----------------------------------------------------------
#  Nanobind headers
# ----------------------------------------------------------

set(NANOBIND_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/nanobind/include PARENT_SCOPE)

# ----------------------------------------------------------
#  Hide a few more settings that aren't relevant for users
# ----------------------------------------------------------

mark_as_advanced(
  BUILD_SHARED_LIBS BUILD_TESTING CMAKE_DEBUG_POSTFIX CMAKE_INSTALL_PREFIX
  CMAKE_VERBOSE_MAKEFILE INSTALL_BIN_DIR INSTALL_INC_DIR INSTALL_LIB_DIR
  INSTALL_MAN_DIR INSTALL_PKGCONFIG_DIR CMAKE_CONFIGURATION_TYPES CMAKE_AR
  CMAKE_VERBOSE_MAKEFILE PYBIND11_INTERNALS_VERSION PYBIND11_SIMPLE_GIL_MANAGEMENT)
