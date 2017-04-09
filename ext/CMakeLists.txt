# Add a CMake parameter for choosing a desired Python version
set(MITSUBA_PYTHON_VERSION "" CACHE STRING "Python version to use for compiling the mitsuba-python library")

if (NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/openexr/OpenEXR")
  message(FATAL_ERROR "The Mitsuba dependencies are missing! "
    "You probably did not clone the project with --recursive. It is possible to recover "
    "by calling \"git submodule update --init --recursive\"")
endif()

# Try to autodetect Python (can be overridden manually if needed)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/pybind11/tools")
set(Python_ADDITIONAL_VERSIONS 3.4 3.5 3.6 3.7)
find_package(PythonLibsNew ${MITSUBA_PYTHON_VERSION})

# Find a matching Python interpreter
if (PYTHONLIBS_FOUND)
  set(PYBIND11_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/pybind11/include ${PYTHON_INCLUDE_DIR} PARENT_SCOPE)
  message(STATUS "Mitsuba: building the Python plugin.")
else()
  message(WARNING "Mitsuba: not building the Python plugin! (won't be able to run the test suite)")
endif()
set(PYTHONLIBS_FOUND ${PYTHONLIBS_FOUND} PARENT_SCOPE)
set(PYTHON_MODULE_PREFIX "${PYTHON_MODULE_PREFIX}" PARENT_SCOPE)
set(PYTHON_MODULE_EXTENSION "${PYTHON_MODULE_EXTENSION}" PARENT_SCOPE)
set(PYTHON_LIBRARIES "${PYTHON_LIBRARIES}" PARENT_SCOPE)

if (WIN32)
  # Build zlib (only on Windows)
  set(ZLIB_BUILD_STATIC_LIBS OFF CACHE BOOL " " FORCE)
  set(ZLIB_BUILD_SHARED_LIBS ON CACHE BOOL " " FORCE)
  add_subdirectory(zlib)

  set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/zlib;${CMAKE_CURRENT_BINARY_DIR}/zlib" CACHE PATH " " FORCE)
  set(ZLIB_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/zlib/$<CONFIGURATION>/zlib.lib" CACHE FILEPATH " " FORCE)

  set_property(TARGET zlib PROPERTY FOLDER "dependencies")
  include_directories(${ZLIB_INCLUDE_DIR} "${CMAKE_CURRENT_BINARY_DIR}/zlib")

  # Build libpng 1.6 (only on Windows)
  set(PNG_SHARED ON CACHE BOOL " " FORCE)
  set(PNG_STATIC OFF CACHE BOOL " " FORCE)
  set(PNG_TESTS OFF CACHE BOOL " " FORCE)
  set(PNG_SKIP_INSTALL_ALL TRUE)
  add_subdirectory(libpng)
  set_property(TARGET png16 PROPERTY FOLDER "dependencies")
  add_dependencies(png16 zlib)

  set(PNG_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/libpng;${CMAKE_CURRENT_BINARY_DIR}/libpng" PARENT_SCOPE)
  set(PNG_LIBRARIES    "$<TARGET_LINKER_FILE:png16>" PARENT_SCOPE)
  set(PNG_DEFINES      -DMTS_HAS_LIBPNG PARENT_SCOPE)

  # Build libjpeg 7 (only on Windows)
  set(LIBJPEG_BUILD_SHARED ON CACHE BOOL " " FORCE)
  set(LIBJPEG_BUILD_EXECUTABLES OFF CACHE BOOL " " FORCE)
  add_subdirectory(libjpeg)
  set_property(TARGET jpeg PROPERTY FOLDER "dependencies")
  set(JPEG_LIBRARIES libjpeg PARENT_SCOPE)

  set(JPEG_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/libjpeg;${CMAKE_CURRENT_BINARY_DIR}/libjpeg" PARENT_SCOPE)
  set(JPEG_LIBRARIES    "$<TARGET_LINKER_FILE:jpeg>" PARENT_SCOPE)
  set(JPEG_DEFINES      -DMTS_HAS_LIBJPEG PARENT_SCOPE)
else()
  # Find system libpng
  find_package(PNG REQUIRED)
  set(PNG_LIBRARIES    ${PNG_LIBRARIES} PARENT_SCOPE)
  set(PNG_INCLUDE_DIRS ${PNG_INCLUDE_DIRS} PARENT_SCOPE)

  # Find system libjpeg
  find_package(JPEG)
  if (JPEG_FOUND)
    set(JPEG_LIBRARIES    ${JPEG_LIBRARIES} PARENT_SCOPE)
    set(JPEG_INCLUDE_DIRS ${JPEG_INCLUDE_DIRS} PARENT_SCOPE)
    set(JPEG_DEFINES      -DMTS_HAS_LIBJPEG PARENT_SCOPE)
  endif()
endif()

# Build OpenEXR
set(ILMBASE_NAMESPACE_VERSIONING OFF CACHE BOOL " " FORCE)
set(OPENEXR_NAMESPACE_VERSIONING OFF CACHE BOOL " " FORCE)
add_subdirectory(openexr)
set_property(TARGET
  IexMath eLut toFloat b44ExpLogTable dwaLookups
  CopyIlmBaseLibs IlmThread Half Iex Imath IlmImf IlmImf-obj
  PROPERTY FOLDER "dependencies")

if (WIN32)
  add_dependencies(IlmImf zlib)
endif()

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

# Use install rpath from this point onwards
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

# Build Thread Building Blocks (main shared libraries only)
set(TBB_BUILD_SHARED          ON  CACHE BOOL " " FORCE)
set(TBB_BUILD_STATIC          OFF CACHE BOOL " " FORCE)
set(TBB_BUILD_TESTS           OFF CACHE BOOL " " FORCE)
set(TBB_BUILD_TBBMALLOC       OFF CACHE BOOL " " FORCE)
set(TBB_BUILD_TBBMALLOC_PROXY OFF CACHE BOOL " " FORCE)
add_subdirectory(tbb)
set_property(TARGET tbb tbb_def_files PROPERTY FOLDER "dependencies")
set(TBB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/tbb/include PARENT_SCOPE)

# Build NanoGUI
set(NANOGUI_BUILD_EXAMPLE OFF CACHE BOOL " " FORCE)
set(NANOGUI_BUILD_SHARED  ON  CACHE BOOL " " FORCE)
set(NANOGUI_BUILD_PYTHON  ON  CACHE BOOL " " FORCE)
set(NANOGUI_INSTALL       OFF CACHE BOOL " " FORCE)

add_subdirectory(nanogui)
set_property(TARGET glfw glfw_objects nanogui nanogui-obj PROPERTY FOLDER "dependencies")
set(EIGEN_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/nanogui/ext/eigen PARENT_SCOPE)
set(NANOGUI_INCLUDE_DIRS
   ${CMAKE_CURRENT_SOURCE_DIR}/nanogui/include
   ${NANOGUI_EXTRA_INCS}
   PARENT_SCOPE)
set(NANOGUI_EXTRA_DEFS ${NANOGUI_EXTRA_DEFS} PARENT_SCOPE)

if (PYTHONLIBS_FOUND)
  set_target_properties(nanogui-python nanogui-python-obj PROPERTIES FOLDER "dependencies")
endif()

# Build the pugixml parser
add_library(pugixml SHARED pugixml/src/pugixml.cpp)
set_property(TARGET pugixml PROPERTY
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pugixml")
set_property(TARGET pugixml PROPERTY FOLDER "dependencies")
set(PUGIXML_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/pugixml/src PARENT_SCOPE)
set_property(SOURCE pugixml/src/pugixml.cpp
  APPEND PROPERTY COMPILE_DEFINITIONS PUGIXML_BUILD_DLL)

# Build ZeroMQ
set(WITH_SODIUM OFF CACHE BOOL " " FORCE)
set(WITH_TWEETNACL OFF CACHE BOOL " " FORCE)
set(WITH_DOC OFF  CACHE BOOL " " FORCE)
set(ZMQ_BUILD_FRAMEWORK OFF CACHE BOOL " " FORCE)
set(ZMQ_BUILD_TESTS OFF CACHE BOOL " " FORCE)
set(LIBZMQ_PEDANTIC OFF CACHE BOOL " " FORCE)
add_subdirectory(zeromq)
set_property(TARGET libzmq PROPERTY FOLDER "dependencies")

# tinyformat include path
set(TINYFORMAT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/tinyformat PARENT_SCOPE)

# Enoki include path
set(ENOKI_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/enoki/include PARENT_SCOPE)

# Build asmjit
set(ASMJIT_BUILD_X64 TRUE)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-inconsistent-missing-override -Wno-undefined-inline")
endif()
add_subdirectory(asmjit)
set(ASMJIT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/asmjit/src PARENT_SCOPE)
set_property(TARGET asmjit PROPERTY FOLDER "dependencies")
