# ----------------------------------------------------------
#   Mitsuba python library
# ----------------------------------------------------------

include_directories(
  ${PNG_INCLUDE_DIRS}
  ${PUGIXML_INCLUDE_DIRS}
  ${ASMJIT_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIR}
  ${OPENEXR_INCLUDE_DIRS}
  ${JPEG_INCLUDE_DIRS}
)

foreach (MI_VARIANT ${MI_VARIANTS})
  string(REPLACE "|" ";" MI_VARIANT ${MI_VARIANT})
  list(GET MI_VARIANT 0 MI_VARIANT_NAME)
  list(GET MI_VARIANT 1 MI_VARIANT_FLOAT)
  list(GET MI_VARIANT 2 MI_VARIANT_SPECTRUM)
  set(TARGET_NAME mitsuba_${MI_VARIANT_NAME}_ext)

  pybind11_add_module(${TARGET_NAME}
      THIN_LTO OPT_SIZE
      main_v.cpp
      ${CORE_PY_V_SRC}
      ${RENDER_PY_V_SRC}
  )

  target_compile_definitions(${TARGET_NAME} PRIVATE
      "-DMI_VARIANT_NAME=${MI_VARIANT_NAME}"
      "-DMI_VARIANT_FLOAT=${MI_VARIANT_FLOAT}"
      "-DMI_VARIANT_SPECTRUM=${MI_VARIANT_SPECTRUM}"
      # macOS: Avoid warnings about loop unrolling being disabled with -Os
      "-DDRJIT_UNROLL= "
  )

  target_link_libraries(${TARGET_NAME} PRIVATE mitsuba)

  set_target_properties(${TARGET_NAME}
      PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${MI_BINARY_DIR}/python/mitsuba
      FOLDER python
  )

  install(TARGETS ${TARGET_NAME}
      LIBRARY DESTINATION mitsuba
  )
endforeach()

pybind11_add_module(mitsuba_ext
  THIN_LTO OPT_SIZE
  main.cpp
  ${CORE_PY_SRC}
  ${RENDER_PY_SRC}
)

# macOS: Avoid warnings about loop unrolling being disabled with -Os
target_compile_definitions(mitsuba_ext PRIVATE "-DDRJIT_UNROLL= ")

target_link_libraries(mitsuba_ext PRIVATE mitsuba)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64)|(amd64)|(AMD64)")
  target_link_libraries(mitsuba_ext PRIVATE asmjit)
endif()

set_target_properties(mitsuba_ext
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${MI_BINARY_DIR}/python/mitsuba
  FOLDER python
)

install(TARGETS mitsuba_ext
  LIBRARY DESTINATION mitsuba
)

# ----------------------------------------------------------
#   Copy & installation targets for files in 'src/python'
# ----------------------------------------------------------

file(GLOB_RECURSE MI_PYTHON_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.py)

set(MI_COPY_FILES "")
foreach(file ${MI_PYTHON_FILES})
  get_filename_component(IN_FILE_DIR ${file} DIRECTORY)
  set(IN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${file})
  ro_copy(${IN_FILE} python/mitsuba/${file})
  install(FILES ${IN_FILE} DESTINATION mitsuba/${IN_FILE_DIR})
endforeach()

# ----------------------------------------------------------
#   Installation targets for auto-generated configuration
# ----------------------------------------------------------

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/../../python/mitsuba/config.py
  DESTINATION mitsuba
)

if (MSVC)
  ro_copy(${CMAKE_CURRENT_BINARY_DIR}/../../python/mitsuba/config.py python/mitsuba/config.py)
endif()

# ----------------------------------------------------------
#   Copy & installation targets for DrJit python bindings
# ----------------------------------------------------------

if (NOT SKBUILD)
  if (MSVC)
    set_target_properties(drjit-python
      PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY_RELEASE        ${MI_BINARY_DIR}/python/drjit
      LIBRARY_OUTPUT_DIRECTORY_DEBUG          ${MI_BINARY_DIR}/python/drjit
      LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${MI_BINARY_DIR}/python/drjit
      LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL     ${MI_BINARY_DIR}/python/drjit)
  else ()
    set_target_properties(drjit-python
      PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${MI_BINARY_DIR}/python/drjit)
  endif()

  set(DRJIT_PYTHON_FILES
    __init__.py const.py detail.py generic.py
    matrix.py router.py traits.py tensor.py
  )

  foreach(file ${DRJIT_PYTHON_FILES})
    set(IN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/drjit/drjit/${file})
    ro_copy(${IN_FILE} python/drjit/${file})
  endforeach(file)
endif()


# ----------------------------------------------------------
#   pytest.ini file
# ----------------------------------------------------------

get_filename_component(MI_TEST_BASE ..
    REALPATH BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(WRITE
  ${CMAKE_CURRENT_BINARY_DIR}/../../pytest.ini
  "[pytest]\n"
  "minversion = 3.0\n"
  "testpaths = \"${MI_TEST_BASE}\"\n"
)

if (MSVC)
  ro_copy(${CMAKE_CURRENT_BINARY_DIR}/../../pytest.ini pytest.ini)
endif()


add_custom_target(copy-python ALL DEPENDS ${MI_COPY_FILES})
set_target_properties(copy-python PROPERTIES FOLDER python)

# ----------------------------------------------------------
#   docstring target
# ----------------------------------------------------------

if (UNIX)
  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/nanogui/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/drjit/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/drjit/ext/drjit-core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/drjit/ext/drjit-core/ext/nanothread/include
  )

  get_filename_component(MKDOC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../include/mitsuba ABSOLUTE)
  get_property(MKDOC_INCLUDE_DIRECTORIES DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
  get_property(MKDOC_COMPILE_DEFINITIONS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY COMPILE_DEFINITIONS)

  foreach (value ${MKDOC_INCLUDE_DIRECTORIES})
    list(APPEND MKDOC_CXXFLAGS -I${value})
  endforeach()

  foreach (value ${MKDOC_COMPILE_DEFINITIONS})
    list(APPEND MKDOC_CXXFLAGS -D${value})
  endforeach()

  add_custom_target(docstrings USES_TERMINAL COMMAND
    ${Python_EXECUTABLE} -m pybind11_mkdoc -std=c++17 -stdlib=libc++ -ferror-limit=100000
    ${MKDOC_CXXFLAGS}
    `find ${MKDOC_PATH}/core -name '*.h' ! -name fwd.h -print`
    `find ${MKDOC_PATH}/render -name '*.h' ! -name fwd.h -print`
    `find ${MKDOC_PATH}/ui -name '*.h' ! -name fwd.h -print`
    -o ${MKDOC_PATH}/python/docstr.h
  )
endif()
