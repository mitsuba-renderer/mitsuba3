# ----------------------------------------------------------
#  Intel Thread Building Blocks (TBB)
# ----------------------------------------------------------

# Build Thread Building Blocks (main shared libraries only)
set(TBB_BUILD_SHARED          ON  CACHE BOOL " " FORCE)
set(TBB_BUILD_STATIC          OFF CACHE BOOL " " FORCE)
set(TBB_BUILD_TESTS           OFF CACHE BOOL " " FORCE)
set(TBB_BUILD_TBBMALLOC       OFF CACHE BOOL " " FORCE)
set(TBB_BUILD_TBBMALLOC_PROXY OFF CACHE BOOL " " FORCE)
set(TBB_INSTALL_TARGETS       OFF CACHE BOOL " " FORCE)
add_subdirectory(tbb)
set_property(TARGET tbb tbb_def_files PROPERTY FOLDER "dependencies")
set(TBB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/tbb/include)
set(TBB_INCLUDE_DIRS ${TBB_INCLUDE_DIRS} PARENT_SCOPE)
set_property(TARGET tbb PROPERTY OUTPUT_NAME "tbb-mitsuba")

# ----------------------------------------------------------
#  Intel ITT (profiler events for VTune, optional)
# ----------------------------------------------------------

if (MTS_ENABLE_PROFILER AND MTS_PROFILER_ITTNOTIFY)
  add_subdirectory(ittnotify/ittnotify)
  set(ITT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/ittnotify/ittnotify/include)
  set(ITT_INCLUDE_DIRS ${ITT_INCLUDE_DIRS} PARENT_SCOPE)
  set_property(TARGET ittnotify PROPERTY FOLDER "dependencies")
  message(STATUS "Mitsuba: emitting profiling events with Intel ITT.")
endif()

# ----------------------------------------------------------
#  Enoki (+ Python bindings, JIT compiler, etc.)
# ----------------------------------------------------------

set(ENOKI_ENABLE_JIT      ${MTS_ENABLE_JIT} CACHE BOOL " " FORCE)
set(ENOKI_ENABLE_AUTODIFF ${MTS_ENABLE_AUTODIFF} CACHE BOOL " " FORCE)
set(ENOKI_ENABLE_PYTHON   ${MTS_ENABLE_PYTHON} CACHE BOOL " " FORCE)
set(ENOKI_PYBIND11_DIR "${CMAKE_CURRENT_SOURCE_DIR}/pybind11" CACHE STRING
    "Path containing the 'pybind11' library used to compile Enoki." FORCE)
set(ENOKI_NATIVE_FLAGS "${MTS_NATIVE_FLAGS}" CACHE STRING
    "Compilation flags used to target the host processor architecture." FORCE)
if (MTS_ENABLE_PROFILER AND MTS_PROFILER_NVTX)
    include_directories(${CUDA_TOOLKIT_ROOT_DIR}/include)
    set(ENOKI_ENABLE_NVTX ON)
endif()

add_subdirectory(enoki)

set_target_properties(enoki-thread PROPERTIES FOLDER "enoki")

if (MTS_ENABLE_JIT)
  set_target_properties(enoki-jit PROPERTIES FOLDER "enoki")
endif()

if (MTS_ENABLE_AUTODIFF)
  set_target_properties(enoki-autodiff PROPERTIES FOLDER "enoki")
endif()

if (MTS_ENABLE_PYTHON)
  set_target_properties(enoki-python PROPERTIES FOLDER "enoki")
endif()

set(ENOKI_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/enoki/include
    ${CMAKE_CURRENT_SOURCE_DIR}/enoki/ext/enoki-jit/include
    ${CMAKE_CURRENT_SOURCE_DIR}/enoki/ext/enoki-jit/ext/enoki-thread/include
    PARENT_SCOPE)

# ----------------------------------------------------------
#  Intel Embree, optional
# ----------------------------------------------------------

if (MTS_ENABLE_EMBREE)
  set(EMBREE_ISPC_SUPPORT           OFF CACHE BOOL " " FORCE)
  set(EMBREE_TUTORIALS              OFF CACHE BOOL " " FORCE)
  set(EMBREE_FILTER_FUNCTION        OFF CACHE BOOL " " FORCE)
  set(EMBREE_IGNORE_CMAKE_CXX_FLAGS OFF CACHE BOOL " " FORCE)
  set(EMBREE_GEOMETRY_QUAD          OFF CACHE BOOL " " FORCE)
  set(EMBREE_GEOMETRY_CURVE         OFF CACHE BOOL " " FORCE)
  set(EMBREE_GEOMETRY_GRID          OFF CACHE BOOL " " FORCE)
  set(EMBREE_GEOMETRY_SUBDIVISION   OFF CACHE BOOL " " FORCE)
  set(EMBREE_GEOMETRY_INSTANCE      ON  CACHE BOOL " " FORCE)
  set(EMBREE_GEOMETRY_USER          ON  CACHE BOOL " " FORCE)
  set(EMBREE_IGNORE_INVALID_RAYS    ON  CACHE BOOL " " FORCE)
  set(EMBREE_RAY_MASK               OFF CACHE BOOL " " FORCE)
  set(EMBREE_MAX_ISA "NONE"         CACHE STRING " " FORCE)

  if (MSVC)
    set(EMBREE_ISA_SSE2             OFF CACHE BOOL " " FORCE)
    set(EMBREE_ISA_SSE42            OFF CACHE BOOL " " FORCE)
    set(EMBREE_ISA_AVX              OFF CACHE BOOL " " FORCE)
    set(EMBREE_ISA_AVX2             OFF CACHE BOOL " " FORCE)
    set(EMBREE_ISA_AVX512SKX        OFF CACHE BOOL " " FORCE)

    if (MTS_NATIVE_FLAGS MATCHES "AVX2")
      set(EMBREE_ISA_AVX2 ON CACHE BOOL " " FORCE)
      set(EMBREE_TARGET "embree_avx2")
    elseif (MTS_NATIVE_FLAGS MATCHES "AVX")
      set(EMBREE_ISA_AVX ON CACHE BOOL " " FORCE)
      set(EMBREE_TARGET "embree_avx")
    else()
      set(EMBREE_ISA_SSE42 ON CACHE BOOL " " FORCE)
      set(EMBREE_TARGET "embree_sse42")
    endif()
  else()
    # Conservative: compile kernels for everyhing
    set(EMBREE_ISA_SSE2             ON CACHE BOOL " " FORCE)
    set(EMBREE_ISA_SSE42            ON CACHE BOOL " " FORCE)
    set(EMBREE_ISA_AVX              ON CACHE BOOL " " FORCE)
    set(EMBREE_ISA_AVX2             ON CACHE BOOL " " FORCE)
    set(EMBREE_ISA_AVX512SKX        ON CACHE BOOL " " FORCE)
  endif()

  set(EMBREE_TASKING_SYSTEM "TBB" CACHE STRING " " FORCE)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable")
  endif()

  # Briefly remove -march=native and let Embree do it's own ISA selection
  set(MTS_CXX_FLAGS_BACKUP "${CMAKE_CXX_FLAGS}")
  string(REPLACE "${MTS_NATIVE_FLAGS}" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REPLACE "/W3" "/W2" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  add_subdirectory(embree)
  set(CMAKE_CXX_FLAGS "${MTS_CXX_FLAGS_BACKUP}")
  set(EMBREE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/embree/include" PARENT_SCOPE)

  if (MSVC)
    # Don't complain about ignored alignment specifier
    target_compile_options(embree PRIVATE "/wd4359")
  endif()

  set_property(TARGET
    algorithms lexers math simd sys tasking embree ${EMBREE_TARGET}
    PROPERTY FOLDER "dependencies/embree")
endif()

# ----------------------------------------------------------
#  OpenEXR (& zlib on Windows)
# ----------------------------------------------------------

if (WIN32)
  set(ZLIB_BUILD_STATIC_LIBS ON CACHE BOOL " " FORCE)
  set(ZLIB_BUILD_SHARED_LIBS ON CACHE BOOL " " FORCE)
  add_subdirectory(zlib)

  set_property(TARGET zlib PROPERTY FOLDER "dependencies")
  set(ZLIB_LIBRARY zlib)
  set(ZLIB_INCLUDE_DIR
      ${CMAKE_CURRENT_SOURCE_DIR}/zlib
      ${CMAKE_CURRENT_BINARY_DIR}/zlib
      CACHE BOOL " " FORCE)
endif()

# Prevents openexr to set the DEBUG_POSTFIX "_d"
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING " " FORCE)
set(ILMBASE_LIB_SUFFIX  "" CACHE STRING "" FORCE)
set(OPENEXR_LIB_SUFFIX  "" CACHE STRING "" FORCE)
set(OPENEXR_BUILD_UTILS        OFF CACHE BOOL "" FORCE)
set(ILMBASE_INSTALL_PKG_CONFIG OFF CACHE BOOL "" FORCE)
set(OPENEXR_INSTALL_PKG_CONFIG OFF CACHE BOOL "" FORCE)
set(PYILMBASE_ENABLE           OFF CACHE BOOL "" FORCE)
add_subdirectory(openexr)

set_property(TARGET
  IexMath IlmThread Half Iex Imath IlmImf IexMath
  PROPERTY FOLDER "dependencies/openexr")

set_property(TARGET
  IexMath IlmThread Half Iex Imath IlmImf IexMath
  PROPERTY PUBLIC_HEADER "")

set(OPENEXR_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/openexr/IlmBase/Imath
  ${CMAKE_CURRENT_SOURCE_DIR}/openexr/IlmBase/Imath
  ${CMAKE_CURRENT_SOURCE_DIR}/openexr/IlmBase/Imath
  ${CMAKE_CURRENT_SOURCE_DIR}/openexr/IlmBase/Iex
  ${CMAKE_CURRENT_SOURCE_DIR}/openexr/IlmBase/Half
  ${CMAKE_CURRENT_SOURCE_DIR}/openexr/OpenEXR/IlmImf
  ${CMAKE_CURRENT_BINARY_DIR}/openexr/OpenEXR/config
  ${CMAKE_CURRENT_BINARY_DIR}/openexr/IlmBase/config
  PARENT_SCOPE
)

# ----------------------------------------------------------
#  libpng16 (On Windows & MacOS)
# ----------------------------------------------------------

if (WIN32 OR APPLE)
  set(PNG_SHARED ON CACHE BOOL " " FORCE)
  set(PNG_STATIC OFF CACHE BOOL " " FORCE)
  set(PNG_TESTS OFF CACHE BOOL " " FORCE)
  set(PNG_SKIP_INSTALL_ALL TRUE)
  add_subdirectory(libpng)
  set_property(TARGET png16 PROPERTY FOLDER "dependencies")
  set_property(TARGET png16 PROPERTY OUTPUT_NAME "png16-mitsuba")

  set(PNG_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/libpng;${CMAKE_CURRENT_BINARY_DIR}/libpng" PARENT_SCOPE)
  set(PNG_LIBRARIES    "png16" PARENT_SCOPE)
  set(PNG_DEFINES      -DMTS_HAS_LIBPNG PARENT_SCOPE)
else()
  # Find system libpng
  find_package(PNG)
  if (PNG_FOUND)
    set(PNG_LIBRARIES    ${PNG_LIBRARIES} PARENT_SCOPE)
    set(PNG_INCLUDE_DIRS ${PNG_INCLUDE_DIRS} PARENT_SCOPE)
    set(PNG_DEFINES      -DMTS_HAS_LIBPNG PARENT_SCOPE)
  else()
    message(WARNING "\n${BoldRed}libpng could not be found: Mitsuba will "
            "not be able to read/write PNG files!${ColorReset}")
  endif()
endif()

# ----------------------------------------------------------
#  libjpeg 7 (On Windows & MacOS)
# ----------------------------------------------------------

if (WIN32 OR APPLE)
  set(LIBJPEG_BUILD_SHARED ON CACHE BOOL " " FORCE)
  set(LIBJPEG_BUILD_EXECUTABLES OFF CACHE BOOL " " FORCE)
  add_subdirectory(libjpeg)
  set_property(TARGET jpeg PROPERTY FOLDER "dependencies")
  set(JPEG_LIBRARIES libjpeg PARENT_SCOPE)

  set(JPEG_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/libjpeg;${CMAKE_CURRENT_BINARY_DIR}/libjpeg" PARENT_SCOPE)
  set(JPEG_LIBRARIES    "jpeg" PARENT_SCOPE)
  set(JPEG_DEFINES      -DMTS_HAS_LIBJPEG PARENT_SCOPE)

  # Give libpng & libjpeg a name that's guaranteeed not to match other
  # libraries that may already be loaded (e.g. into a Python interpreter)
  set_property(TARGET jpeg PROPERTY OUTPUT_NAME "jpeg-mitsuba")
else()
  # Find system libjpeg
  find_package(JPEG)
  if (JPEG_FOUND)
    set(JPEG_LIBRARIES    ${JPEG_LIBRARIES} PARENT_SCOPE)
    set(JPEG_INCLUDE_DIRS ${JPEG_INCLUDE_DIRS} PARENT_SCOPE)
    set(JPEG_DEFINES      -DMTS_HAS_LIBJPEG PARENT_SCOPE)
  else()
    message(WARNING "\n${BoldRed}libjpeg could not be found: Mitsuba will "
            "not be able to read/write JPEG files!${ColorReset}")
  endif()
endif()

# if (MTS_ENABLE_GUI)
#     set(NANOGUI_BUILD_EXAMPLES OFF CACHE BOOL " " FORCE)
#     set(NANOGUI_BUILD_SHARED   ON  CACHE BOOL " " FORCE)
#     set(NANOGUI_BUILD_PYTHON   ${MTS_ENABLE_PYTHON} CACHE BOOL " " FORCE)
#     set(NANOGUI_INSTALL        OFF CACHE BOOL " " FORCE)
#     set(NANOGUI_PYBIND11_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/pybind11")
#     add_subdirectory(nanogui)
#
#     set_property(TARGET glfw glfw_objects nanogui PROPERTY FOLDER "dependencies")
#     set(EIGEN_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/nanogui/ext/eigen PARENT_SCOPE)
#     set(NANOGUI_INCLUDE_DIRS
#     ${CMAKE_CURRENT_SOURCE_DIR}/nanogui/include
#     ${NANOGUI_EXTRA_INCS}
#     PARENT_SCOPE)
#     set(NANOGUI_EXTRA_DEFS ${NANOGUI_EXTRA_DEFS} PARENT_SCOPE)
#
#     if (MTS_ENABLE_PYTHON)
#         set_target_properties(nanogui-python PROPERTIES FOLDER "dependencies")
#     endif()
# endif()

# ----------------------------------------------------------
#  pugixml XML parser
# ----------------------------------------------------------

add_library(pugixml SHARED pugixml/src/pugixml.cpp)
set_property(TARGET pugixml PROPERTY
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pugixml")
set_property(TARGET pugixml PROPERTY FOLDER "dependencies")
set(PUGIXML_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/pugixml/src PARENT_SCOPE)
set_property(SOURCE pugixml/src/pugixml.cpp
  APPEND PROPERTY COMPILE_DEFINITIONS PUGIXML_BUILD_DLL)


# tinyformat include path
set(TINYFORMAT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/tinyformat PARENT_SCOPE)

# ----------------------------------------------------------
#  asmjit -- x86/64 JIT compiler
# ----------------------------------------------------------

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64|AMD64")
  # Build asmjit
  set(ASMJIT_BUILD_X64 TRUE)
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

# ----------------------------------------------------------
#  sRGB spectral upsampling model
# ----------------------------------------------------------

# Disable annoying MSVC warnings in rgb2spec build
if (MSVC)
  add_definitions(/D "_CRT_SECURE_NO_WARNINGS")
endif()

# build rgb2spec
add_subdirectory(rgb2spec)
set(RGB2SPEC_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/rgb2spec PARENT_SCOPE)
set_target_properties(rgb2spec rgb2spec_opt rgb2spec_opt_run PROPERTIES FOLDER "dependencies/rgb2spec")

mark_as_advanced(
  ASMJIT_BUILD_X86 ASMJIT_BUILD_ARM ASMJIT_BUILD_TEST ASMJIT_DIR ASMJIT_EMBED
  ASMJIT_STATIC EMBREE_ADDRESS_SANITIZER EMBREE_API_NAMESPACE
  EMBREE_BACKFACE_CULLING EMBREE_COMPACT_POLYS EMBREE_FILTER_FUNCTION
  EMBREE_GEOMETRY_CURVE EMBREE_GEOMETRY_GRID EMBREE_GEOMETRY_INSTANCE
  EMBREE_GEOMETRY_POINT EMBREE_GEOMETRY_QUAD EMBREE_GEOMETRY_SUBDIVISION
  EMBREE_GEOMETRY_TRIANGLE EMBREE_GEOMETRY_USER EMBREE_IGNORE_CMAKE_CXX_FLAGS
  EMBREE_IGNORE_INVALID_RAYS EMBREE_ISPC_SUPPORT EMBREE_LIBRARY_NAME
  EMBREE_MAX_INSTANCE_LEVEL_COUNT EMBREE_MAX_ISA EMBREE_RAY_MASK
  EMBREE_ISA_SSE2 EMBREE_ISA_SSE42 EMBREE_ISA_AVX EMBREE_ISA_AVX2 EMBREE_ISA_AVX512SKX  EMBREE_ISA_AVX512KNL
  EMBREE_STACK_PROTECTOR EMBREE_STATIC_LIB EMBREE_STAT_COUNTERS
  EMBREE_TUTORIALS EMBREE_CURVE_SELF_INTERSECTION_AVOIDANCE_FACTOR
  EMBREE_MAX_INSTANCE_LEVEL_COUN EMBREE_MIN_WIDTH EMBREE_RAY_PACKETS
  EMBREE_TASKING_SYSTEM ENOKI_ENABLE_AUTODIFF ENOKI_ENABLE_JIT
  ENOKI_ENABLE_PYTHON ENOKI_ENABLE_TESTS ENOKI_NATIVE_FLAGS
  ILMBASE_BUILD_SHARED_LIBS ILMBASE_NAMESPACE_VERSIONING ILMIMF_BUILD_TESTS
  ILMIMF_CREATE_LIBTOOL_FILE ILMIMF_INSTALL_PKGCONFIG LIBJPEG_BUILD_EXECUTABLES
  LIBJPEG_BUILD_SHARED M_LIBRARY OPENEXR_BUILD_TESTS
  OPENEXR_BUILD_EXAMPLES OPENEXR_BUILD_SHARED_LIBS
  OPENEXR_BUILD_UTILS OPENEXR_INSTALL_DOCS OPENEXR_INSTALL_EXAMPLES
  OPENEXR_NAMESPACE_VERSIONING OPENEXR_USE_ZLIB_WINAPI
  PNGARG PNG_DEBUG PNG_FRAMEWORK PNG_SHARED PNG_STATIC PNG_TESTS
  TBB_BUILD_PYTHON TBB_BUILD_SHARED TBB_BUILD_STATIC TBB_BUILD_TBBMALLOC
  TBB_BUILD_TBBMALLOC_PROXY TBB_BUILD_TESTS TBB_CONFIG_DATE TBB_INSTALL_TARGETS
  TBB_NO_DATE TBB_SET_SOVERSION CMAKE_CONFIGURATION_TYPES
  PYBIND11_INSTALL PYBIND11_TEST ZLIB_LIBRARIES
  jconfig_dir
)
