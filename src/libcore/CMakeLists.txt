include_directories(
  ${PNG_INCLUDE_DIRS}
)
add_definitions(
  ${PNG_DEFINES}
  -DMTS_BUILD_MODULE=MTS_MODULE_CORE
)

add_library(mitsuba-core SHARED 
   object.cpp
   properties.cpp
   #path.cpp
)
target_link_libraries(mitsuba-core ${PNG_LIBRARIES})

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
