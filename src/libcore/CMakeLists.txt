include_directories(
  ${PNG_INCLUDE_DIRS}
  ${TBB_INCLUDE_DIRS}
  ${PCG32_INCLUDE_DIRS}
  ${TINYFORMAT_INCLUDE_DIRS}
)

add_definitions(
  ${PNG_DEFINES}
  -DMTS_BUILD_MODULE=MTS_MODULE_CORE
)

set(INC_DIR "../../include/mitsuba/core")

add_library(mitsuba-core SHARED
  ${INC_DIR}/../mitsuba.h
  ${INC_DIR}/atomic.h
  ${INC_DIR}/fwd.h
  ${INC_DIR}/essentials.h
  ${INC_DIR}/platform.h
  ${INC_DIR}/variant.h
  ${INC_DIR}/vector.h
  ${INC_DIR}/atomic.h
  ${INC_DIR}/string.h

  appender.cpp     ${INC_DIR}/appender.h
  class.cpp        ${INC_DIR}/class.h
  filesystem.cpp   ${INC_DIR}/filesystem.h
  formatter.cpp    ${INC_DIR}/formatter.h
  logger.cpp       ${INC_DIR}/logger.h
  math.cpp         ${INC_DIR}/math.h
  object.cpp       ${INC_DIR}/object.h
  properties.cpp   ${INC_DIR}/properties.h
  thread.cpp       ${INC_DIR}/thread.h
  tls.cpp          ${INC_DIR}/tls.h
  util.cpp         ${INC_DIR}/util.h
  argparser.cpp    ${INC_DIR}/argparser.h
)

target_link_libraries(mitsuba-core ${PNG_LIBRARIES} tbb)

# Copy libcore binary to the 'dist' directory
add_custom_command(
  TARGET mitsuba-core
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
)

set(DEPENDENCIES pugixml IlmThread Half Half Imath IlmImf Iex nanogui mitsuba-core tbb libzmq)
if (WIN32)
  list(APPEND DEPENDENCIES png16 zlib jpeg)
endif()

# Copy dependencies into the 'dist' directory
foreach(dep ${DEPENDENCIES})
  add_custom_command(
    TARGET mitsuba-core
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${dep}> ${CMAKE_BINARY_DIR}/dist)
endforeach(dep)
