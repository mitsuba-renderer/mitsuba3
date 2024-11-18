# ----------------------------------------------------------
#   Mitsuba python library
# ----------------------------------------------------------

  set(MI_DIST_FLAGS "")
if (SKBUILD)
  # Enable LTO only for release builds targeting PyPI (~5% binary size reduction)
  set(MI_DIST_FLAGS LTO)

  if (MI_STABLE_ABI)
    list(APPEND MI_DIST_FLAGS STABLE_ABI)
  endif()
endif()

set(MI_NB_DOMAIN drjit)

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
  set(TARGET_NAME ${MI_VARIANT_NAME})

  nanobind_add_module(${TARGET_NAME}
      NB_DOMAIN ${MI_NB_DOMAIN}
      NB_SHARED
      ${MI_DIST_FLAGS}
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

set(nanobind_lib_name "nanobind")
if (STABLE_ABI IN_LIST MI_DIST_FLAGS)
    set(nanobind_lib_name "${nanobind_lib_name}-abi3")
endif()
set(nanobind_lib_name "${nanobind_lib_name}-${MI_NB_DOMAIN}") # Append domain name
set_target_properties(${nanobind_lib_name}
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${MI_BINARY_DIR}/python/mitsuba
  RUNTIME_OUTPUT_DIRECTORY ${MI_BINARY_DIR}/python/mitsuba
  FOLDER python
)
install(TARGETS ${nanobind_lib_name}
  LIBRARY DESTINATION mitsuba
)

nanobind_add_module(mitsuba_ext
  NB_DOMAIN drjit
  NB_SHARED
  ${MI_DIST_FLAGS}
  main.cpp
  ${CORE_PY_SRC}
  ${RENDER_PY_SRC}
)

nanobind_add_module(mitsuba_alias
  NB_DOMAIN drjit
  NB_SHARED
  ${MI_DIST_FLAGS}
  alias.cpp
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

set_target_properties(mitsuba_alias
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${MI_BINARY_DIR}/python/mitsuba
  FOLDER python
)

target_include_directories(mitsuba_alias
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/drjit/include 
  ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/drjit/ext/drjit-core/include)

install(TARGETS mitsuba_alias
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
#   Installation targets for DrJit python bindings
# ----------------------------------------------------------

if (NOT SKBUILD)
  if (MSVC)
    set_target_properties(drjit-python
      PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY_RELEASE        ${MI_BINARY_DIR}/python/drjit
      LIBRARY_OUTPUT_DIRECTORY_DEBUG          ${MI_BINARY_DIR}/python/drjit
      LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${MI_BINARY_DIR}/python/drjit
      LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL     ${MI_BINARY_DIR}/python/drjit)
    target_compile_options(drjit-python PRIVATE /wd4324) # structure was padded due to alignment specified
  else ()
    set_target_properties(drjit-python
      PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${MI_BINARY_DIR}/python/drjit)
  endif()
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

# ----------------------------------------------------------
#   Copy python source files
# ----------------------------------------------------------

add_custom_target(mitsuba-copy-python-src ALL DEPENDS ${MI_COPY_FILES})
set_target_properties(mitsuba-copy-python-src PROPERTIES FOLDER python)

set(MI_ALL_VARIANTS "")

foreach (MI_VARIANT ${MI_VARIANTS})
  string(REPLACE "|" ";" MI_VARIANT ${MI_VARIANT})
  list(GET MI_VARIANT 0 MI_VARIANT_NAME)
  list(APPEND MI_ALL_VARIANTS ${MI_VARIANT_NAME})
endforeach()

set(MI_STUB_DEPENDS mitsuba_alias mitsuba_ext mitsuba-copy-python-src ${MI_ALL_VARIANTS})

# ----------------------------------------------------------
#   Copy targets for python (no rpath on Windows)
# ----------------------------------------------------------

if (MSVC)
  # This target is dependent on the root CMakeLists.txt target `copy-targets`
  add_custom_target(copy-targets-python ALL)
  set_target_properties(copy-targets-python PROPERTIES FOLDER python)

  list(APPEND MI_STUB_DEPENDS copy-targets-python)
endif()

# ----------------------------------------------------------
#   Generate type information stubs (mitsuba)
# ----------------------------------------------------------

set(STUB_ARGS
  PYTHON_PATH ${MI_BINARY_DIR}/python
  PATTERN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/stubs.pat
  DEPENDS ${MI_STUB_DEPENDS}
)

# The sanitizers require preloading shared libraries into Python, and that
# doesn't play well with stub generation. Disable stubs if sanitizers are
# enabled.
if (NOT (MI_SANITIZE_ADDRESS OR MI_SANITIZE_MEMORY))

  set(MI_STUB_SUBMODULES "math" "quad" "spline" "mueller" "warp" "misc" "detail" "filesystem"
                         "python.ad" "python.chi2" "python.math_py" "python.util" "python.xml")

  nanobind_add_stub(
    mitsuba-stub
    MODULE mitsuba.mitsuba_stubs
    OUTPUT ${MI_BINARY_DIR}/python/mitsuba/_stubs.pyi
    MARKER_FILE ${MI_BINARY_DIR}/python/mitsuba/py.typed
    ${STUB_ARGS}
  )

  add_custom_command(
    OUTPUT ${MI_BINARY_DIR}/python/mitsuba/__init__.pyi
    DEPENDS mitsuba-stub
    COMMAND cmake -P ${CMAKE_CURRENT_SOURCE_DIR}/../../resources/variant-stub.cmake
      ${MI_BINARY_DIR}/python/mitsuba/_stubs.pyi
      ${MI_BINARY_DIR}/python/mitsuba/__init__.pyi
  )

  set(MI_STUB_TARGET_DEPS
    ${MI_BINARY_DIR}/python/mitsuba/__init__.pyi
    ${MI_BINARY_DIR}/python/mitsuba/py.typed)

  foreach (value ${MI_STUB_SUBMODULES})
    string(REPLACE "." "/" value_path ${value})
    nanobind_add_stub(
      mitsuba-stub-${value}
      MODULE mitsuba.mitsuba_stubs.${value}
      OUTPUT ${MI_BINARY_DIR}/python/mitsuba/_${value_path}_stubs.pyi
      ${STUB_ARGS}
    )

    add_custom_command(
      OUTPUT ${MI_BINARY_DIR}/python/mitsuba/${value_path}.pyi
      DEPENDS mitsuba-stub-${value}
      COMMAND cmake -P ${CMAKE_CURRENT_SOURCE_DIR}/../../resources/variant-stub.cmake
        ${MI_BINARY_DIR}/python/mitsuba/_${value_path}_stubs.pyi
        ${MI_BINARY_DIR}/python/mitsuba/${value_path}.pyi
    )

    list(APPEND MI_STUB_TARGET_DEPS ${MI_BINARY_DIR}/python/mitsuba/${value_path}.pyi)
  endforeach()

  add_custom_target(
    mitsuba-stubs
    ALL
    DEPENDS ${MI_STUB_TARGET_DEPS}
  )

  install(FILES ${MI_STUB_TARGET_DEPS} DESTINATION mitsuba)

endif()


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
