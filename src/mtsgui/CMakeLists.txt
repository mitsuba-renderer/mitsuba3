include_directories(
  ${TBB_INCLUDE_DIRS}
  ${ASMJIT_INCLUDE_DIRS}
  ${NANOGUI_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(
  ${NANOGUI_EXTRA_DEFS}
)

add_definitions(-DMTS_BRANCH="${GIT_BRANCH}")
add_definitions(-DMTS_HASH="${GIT_COMMIT_HASH}")

# Glob up resource files
file(GLOB resources "${PROJECT_SOURCE_DIR}/resources/*.png")

# Concatenate resource files into a comma separated string
string (REGEX REPLACE "([^\\]|^);" "\\1," resources_string "${resources}")
string (REGEX REPLACE "[\\](.)" "\\1" resources_string "${resources_string}")

# Create command line for running bin2c cmake script
set(bin2c_cmdline
  -DOUTPUT_C=mtsgui_resources.cpp
  -DOUTPUT_H=mtsgui_resources.h
  "-DINPUT_FILES=${resources_string}"
  -P "${PROJECT_SOURCE_DIR}/ext/nanogui/resources/bin2c.cmake")

# Run bin2c on resource files
add_custom_command(
  OUTPUT mtsgui_resources.cpp mtsgui_resources.h
  COMMAND ${CMAKE_COMMAND} ARGS ${bin2c_cmdline}
  DEPENDS ${resources}
  COMMENT "Running bin2c"
  PRE_BUILD VERBATIM)

add_executable(mtsgui mtsgui.cpp mtsgui_resources.cpp mtsgui_resources.h)

target_link_libraries(mtsgui PRIVATE mitsuba-core mitsuba-render mitsuba-ui tbb)

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64|AMD64")
  target_link_libraries(mtsgui PRIVATE asmjit)
endif()

add_dist(mtsgui)

if (APPLE)
  set_target_properties(mtsgui PROPERTIES INSTALL_RPATH "@executable_path")
endif()
