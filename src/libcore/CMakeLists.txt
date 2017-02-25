include_directories(
  ${HYPOTHESIS_INCLUDE_DIRS}
  ${PNG_INCLUDE_DIRS}
  ${PCG32_INCLUDE_DIRS}
  ${PUGIXML_INCLUDE_DIRS}
  ${ASMJIT_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIR}
  ${OPENEXR_INCLUDE_DIRS}
  ${JPEG_INCLUDE_DIRS}
)

add_definitions(
  ${PNG_DEFINES}
  -DMTS_BUILD_MODULE=MTS_MODULE_CORE
)

set(INC_DIR "../../include/mitsuba/core")

add_library(mitsuba-core-obj OBJECT
  ${INC_DIR}/../mitsuba.h
  ${INC_DIR}/fwd.h
  ${INC_DIR}/essentials.h
  ${INC_DIR}/platform.h
  ${INC_DIR}/variant.h
  ${INC_DIR}/vector.h
  ${INC_DIR}/atomic.h
  ${INC_DIR}/string.h
  ${INC_DIR}/frame.h
  ${INC_DIR}/hash.h
  ${INC_DIR}/jit.h

  appender.cpp         ${INC_DIR}/appender.h
  class.cpp            ${INC_DIR}/class.h
  filesystem.cpp       ${INC_DIR}/filesystem.h
  formatter.cpp        ${INC_DIR}/formatter.h
  logger.cpp           ${INC_DIR}/logger.h
  math.cpp             ${INC_DIR}/math.h
  object.cpp           ${INC_DIR}/object.h
  properties.cpp       ${INC_DIR}/properties.h
  thread.cpp           ${INC_DIR}/thread.h
  tls.cpp              ${INC_DIR}/tls.h
  util.cpp             ${INC_DIR}/util.h
  argparser.cpp        ${INC_DIR}/argparser.h
  xml.cpp              ${INC_DIR}/xml.h
  plugin.cpp           ${INC_DIR}/plugin.h
  fresolver.cpp        ${INC_DIR}/fresolver.h
  jit.cpp              ${INC_DIR}/jit.h
  struct.cpp           ${INC_DIR}/struct.h
  bitmap.cpp           ${INC_DIR}/bitmap.h
  warp.cpp             ${INC_DIR}/warp.h
  random.cpp           ${INC_DIR}/random.h
  qmc.cpp              ${INC_DIR}/qmc.h
  transform.cpp        ${INC_DIR}/transform.h
  bbox.cpp             ${INC_DIR}/bbox.h

  stream.cpp           ${INC_DIR}/stream.h ${INC_DIR}/detail/stream.hpp
  astream.cpp          ${INC_DIR}/astream.h ${INC_DIR}/dstream.h
  fstream.cpp          ${INC_DIR}/fstream.h
  mstream.cpp          ${INC_DIR}/mstream.h
  zstream.cpp          ${INC_DIR}/zstream.h
)

add_library(mitsuba-core SHARED $<TARGET_OBJECTS:mitsuba-core-obj>)
set_property(TARGET mitsuba-core-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(mitsuba-core-obj mitsuba-core PROPERTIES FOLDER mitsuba-core)

target_link_libraries(mitsuba-core PRIVATE 
  # Link to libpng and zlib (either the system version or a version built via cmake)
  ${PNG_LIBRARIES}
  ${ZLIB_LIBRARY}
  # Link to Intel's Thread Building Blocks, the pugixml parser, and asmjit
  tbb pugixml asmjit
  # Image libraries: link to libjpeg, libpng, OpenEXR
  ${PNG_LIBRARIES} ${JPEG_LIBRARIES} IlmImf
)
#
if (UNIX AND NOT APPLE)
  target_link_libraries(mitsuba-core PUBLIC
    -Wl,-rpath-link=$<TARGET_FILE_DIR:tbb>
    -Wl,-rpath-link=$<TARGET_FILE_DIR:pugixml>
    -Wl,-rpath-link=$<TARGET_FILE_DIR:asmjit>)
endif()

# Copy to 'dist' directory
add_dist(mitsuba-core pugixml IlmThread Half Half Imath IlmImf Iex tbb libzmq asmjit)

if (WIN32)
  add_dist(zlib jpeg png16)
  add_dependencies(mitsuba-core zlib jpeg png16)
endif()

