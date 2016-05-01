include_directories(
  ${PNG_INCLUDE_DIRS}
  ${TBB_INCLUDE_DIRS}
  ${PCG32_INCLUDE_DIRS}
)

add_definitions(
  ${PNG_DEFINES}
  -DMTS_BUILD_MODULE=MTS_MODULE_CORE
)

set(INC_DIR "../../include/mitsuba/core")

add_library(mitsuba-core SHARED 
  ${INC_DIR}/../mitsuba.h
  ${INC_DIR}/platform.h
  object.cpp       ${INC_DIR}/object.h
  class.cpp        ${INC_DIR}/class.h
  thread.cpp       ${INC_DIR}/thread.h
  tls.cpp          ${INC_DIR}/tls.h
  properties.cpp   ${INC_DIR}/properties.h
  logger.cpp       ${INC_DIR}/logger.h
  appender.cpp     ${INC_DIR}/appender.h
  formatter.cpp    ${INC_DIR}/formatter.h
  util.cpp         ${INC_DIR}/util.h
)

target_link_libraries(mitsuba-core ${PNG_LIBRARIES} tbb)

add_custom_command(
  TARGET mitsuba-core
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
)

set(DEPENDENCIES pugixml IlmThread Half Half Imath IlmImf Iex nanogui mitsuba-core tbb libzmq)
if (WIN32)
  list(APPEND DEPENDENCIES png16 zlib jpeg)
endif()

# Copy binaries into the 'dist' directory
foreach(dep ${DEPENDENCIES})
  add_custom_command(
    TARGET mitsuba-core
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${dep}> ${CMAKE_BINARY_DIR}/dist)
endforeach(dep)
