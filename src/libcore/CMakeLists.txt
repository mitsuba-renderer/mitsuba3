include_directories(
  ${PNG_INCLUDE_DIRS}
  ${PUGIXML_INCLUDE_DIRS}
  ${ASMJIT_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIR}
  ${OPENEXR_INCLUDE_DIRS}
  ${JPEG_INCLUDE_DIRS}
)

set(INC_DIR "../../include/mitsuba/core")

add_library(mitsuba-core-obj OBJECT
  ${INC_DIR}/../mitsuba.h
  ${INC_DIR}/atomic.h
  ${INC_DIR}/frame.h
  ${INC_DIR}/fwd.h
  ${INC_DIR}/hash.h
  ${INC_DIR}/math.h
  ${INC_DIR}/jit.h
  ${INC_DIR}/simd.h
  ${INC_DIR}/platform.h
  ${INC_DIR}/variant.h

  string.cpp           ${INC_DIR}/string.h
  appender.cpp         ${INC_DIR}/appender.h
  argparser.cpp        ${INC_DIR}/argparser.h
                       ${INC_DIR}/bbox.h
  bitmap.cpp           ${INC_DIR}/bitmap.h
                       ${INC_DIR}/bsphere.h
  class.cpp            ${INC_DIR}/class.h
                       ${INC_DIR}/distr_1d.h
                       ${INC_DIR}/distr_2d.h
  dstream.cpp          ${INC_DIR}/dstream.h
  filesystem.cpp       ${INC_DIR}/filesystem.h
  formatter.cpp        ${INC_DIR}/formatter.h
  fresolver.cpp        ${INC_DIR}/fresolver.h
  fstream.cpp          ${INC_DIR}/fstream.h
  jit.cpp              ${INC_DIR}/jit.h
  logger.cpp           ${INC_DIR}/logger.h
  mmap.cpp             ${INC_DIR}/mmap.h
  tensor.cpp           ${INC_DIR}/tensor.h
  mstream.cpp          ${INC_DIR}/mstream.h
  object.cpp           ${INC_DIR}/object.h
  plugin.cpp           ${INC_DIR}/plugin.h
  profiler.cpp         ${INC_DIR}/profiler.h
  progress.cpp         ${INC_DIR}/progress.h
  properties.cpp       ${INC_DIR}/properties.h
  qmc.cpp              ${INC_DIR}/qmc.h
                       ${INC_DIR}/random.h
                       ${INC_DIR}/ray.h
  rfilter.cpp          ${INC_DIR}/rfilter.h
  spectrum.cpp         ${INC_DIR}/spectrum.h
                       ${INC_DIR}/spline.h
  stream.cpp           ${INC_DIR}/stream.h
  struct.cpp           ${INC_DIR}/struct.h
  thread.cpp           ${INC_DIR}/thread.h
  tls.cpp              ${INC_DIR}/tls.h
  transform.cpp        ${INC_DIR}/transform.h
  util.cpp             ${INC_DIR}/util.h
                       ${INC_DIR}/vector.h
                       ${INC_DIR}/warp.h
  xml.cpp              ${INC_DIR}/xml.h
  zstream.cpp          ${INC_DIR}/zstream.h
  quad.cpp             ${INC_DIR}/quad.h

  dither-matrix256.cpp
)

set_property(
  SOURCE util.cpp
  PROPERTY COMPILE_DEFINITIONS
  MTS_BRANCH="${GIT_BRANCH}" MTS_HASH="${GIT_COMMIT_HASH}"
)

add_library(mitsuba-core SHARED $<TARGET_OBJECTS:mitsuba-core-obj>)
set_property(TARGET mitsuba-core-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(mitsuba-core-obj mitsuba-core PROPERTIES FOLDER mitsuba-core)
target_compile_definitions(mitsuba-core-obj PRIVATE ${PNG_DEFINES} -DMTS_BUILD_MODULE=MTS_MODULE_CORE)

target_link_libraries(mitsuba-core PRIVATE
  # Link to libpng and zlib (either the system version or a version built via cmake)
  ${PNG_LIBRARIES}
  ${ZLIB_LIBRARY}
  # Link to Intel's Thread Building Blocks and the pugixml parser
  tbb pugixml
  # Image libraries: link to libjpeg, libpng, OpenEXR
  ${PNG_LIBRARIES} ${JPEG_LIBRARIES} IlmImf
)
#
if (UNIX AND NOT APPLE)
  target_link_libraries(mitsuba-core PUBLIC
    -Wl,-rpath-link=$<TARGET_FILE_DIR:tbb>
    -Wl,-rpath-link=$<TARGET_FILE_DIR:pugixml>)
endif()

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64|AMD64")
  target_link_libraries(mitsuba-core PRIVATE asmjit)
  if (UNIX AND NOT APPLE)
    target_link_libraries(mitsuba-core PUBLIC
      -Wl,-rpath-link=$<TARGET_FILE_DIR:asmjit>)
  endif()
  add_dist(asmjit)
endif()

# Copy to 'dist' directory
add_dist(mitsuba-core pugixml IlmThread Half Half Imath IlmImf Iex tbb)

if (CMAKE_CXX_COMPILER_ID MATCHES "^(GNU)$")
  target_link_libraries(mitsuba-core PRIVATE -Wl,--no-undefined)
endif()

# Python bindings
if (MTS_ENABLE_PYTHON)
  add_subdirectory(python)
endif()

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)

# Windows-only dependencies
if (WIN32)
  add_dist(zlib jpeg png16)
  add_dependencies(mitsuba-core zlib jpeg png16)
endif()

# Windows & Mac dependencies
if (WIN32 OR APPLE)
  add_dist(jpeg png16)
  add_dependencies(mitsuba-core jpeg png16)
endif()

if (MTS_ENABLE_OPTIX)
  add_dist(enoki-autodiff enoki-cuda)
  target_link_libraries(mitsuba-core PUBLIC enoki-autodiff enoki-cuda cuda)
endif()
