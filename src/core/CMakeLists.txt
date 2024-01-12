include_directories(
  ${PNG_INCLUDE_DIRS}
  ${PUGIXML_INCLUDE_DIRS}
  ${ASMJIT_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIR}
  ${OPENEXR_INCLUDE_DIRS}
  ${JPEG_INCLUDE_DIRS}
)

set(INC_DIR "../../include/mitsuba/core")

add_library(mitsuba-core OBJECT
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

  string.cpp        ${INC_DIR}/string.h
  appender.cpp      ${INC_DIR}/appender.h
  argparser.cpp     ${INC_DIR}/argparser.h
                    ${INC_DIR}/bbox.h
  bitmap.cpp        ${INC_DIR}/bitmap.h
                    ${INC_DIR}/bsphere.h
  class.cpp         ${INC_DIR}/class.h
                    ${INC_DIR}/distr_1d.h
                    ${INC_DIR}/distr_2d.h
  dstream.cpp       ${INC_DIR}/dstream.h
  filesystem.cpp    ${INC_DIR}/filesystem.h
  formatter.cpp     ${INC_DIR}/formatter.h
  fresolver.cpp     ${INC_DIR}/fresolver.h
  fstream.cpp       ${INC_DIR}/fstream.h
  jit.cpp           ${INC_DIR}/jit.h
  logger.cpp        ${INC_DIR}/logger.h
  mmap.cpp          ${INC_DIR}/mmap.h
  tensor.cpp        ${INC_DIR}/tensor.h
  mstream.cpp       ${INC_DIR}/mstream.h
  object.cpp        ${INC_DIR}/object.h
  plugin.cpp        ${INC_DIR}/plugin.h
  profiler.cpp      ${INC_DIR}/profiler.h
  progress.cpp      ${INC_DIR}/progress.h
  properties.cpp    ${INC_DIR}/properties.h
  qmc.cpp           ${INC_DIR}/qmc.h
                    ${INC_DIR}/random.h
                    ${INC_DIR}/ray.h
  rfilter.cpp       ${INC_DIR}/rfilter.h
  spectrum.cpp      ${INC_DIR}/spectrum.h
                    ${INC_DIR}/spline.h
  stream.cpp        ${INC_DIR}/stream.h
  struct.cpp        ${INC_DIR}/struct.h
  thread.cpp        ${INC_DIR}/thread.h
                    ${INC_DIR}/timer.h
  transform.cpp     ${INC_DIR}/transform.h
                    ${INC_DIR}/traits.h
  util.cpp          ${INC_DIR}/util.h
                    ${INC_DIR}/vector.h
                    ${INC_DIR}/warp.h
  xml.cpp           ${INC_DIR}/xml.h
  zstream.cpp       ${INC_DIR}/zstream.h
                    ${INC_DIR}/quad.h

  dither-matrix256.cpp
)

set_property(
  SOURCE util.cpp
  PROPERTY COMPILE_DEFINITIONS
  MI_BRANCH="${GIT_BRANCH}" MI_HASH="${GIT_COMMIT_HASH}"
)

target_compile_definitions(mitsuba-core
  PRIVATE ${PNG_DEFINES} -DMI_BUILD_MODULE=MI_MODULE_LIB)

set_target_properties(mitsuba-core PROPERTIES
  INTERPROCEDURAL_OPTIMIZATION ON)

set_target_properties(mitsuba-core PROPERTIES
  POSITION_INDEPENDENT_CODE ON)

target_link_libraries(mitsuba-core PRIVATE
  # Link to libpng and zlib (either the system version or a version built via cmake)
  ${PNG_LIBRARIES}
  ${ZLIB_LIBRARY}
  nanothread
  # Link pugixml parser
  pugixml
  # Image libraries: link to libjpeg, libpng, OpenEXR
  ${PNG_LIBRARIES} ${JPEG_LIBRARIES} IlmImf
)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64)|(amd64)|(AMD64)")
  target_link_libraries(mitsuba-core PRIVATE asmjit)
endif()

if (NOT MSVC)
  target_link_libraries(mitsuba-core PRIVATE ${CMAKE_DL_LIBS})
endif()

target_link_libraries(mitsuba-core PUBLIC drjit)
target_link_libraries(mitsuba-core PRIVATE fast_float)

if (MI_ENABLE_JIT)
  target_link_libraries(mitsuba-core PUBLIC drjit-core)
endif()

if (MI_ENABLE_AUTODIFF)
  target_link_libraries(mitsuba-core PUBLIC drjit-extra)
endif()

# Python bindings
if (MI_ENABLE_PYTHON)
  add_subdirectory(python)
  set(CORE_PY_V_SRC ${CORE_PY_V_SRC} PARENT_SCOPE)
  set(CORE_PY_SRC   ${CORE_PY_SRC}   PARENT_SCOPE)
endif()
