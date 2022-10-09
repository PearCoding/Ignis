CPMAddPackage(
    NAME pybind11
    GITHUB_REPOSITORY pybind/pybind11 
    GIT_TAG stable
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "PYBIND11_FINDPYTHON ON"
)

set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/api")

add_library(pyignis MODULE api.cpp)
target_link_libraries(pyignis PRIVATE ig_lib_runtime pybind11::module pybind11::lto pybind11::windows_extras)
target_compile_definitions(pyignis PRIVATE IGNIS_VERSION=${PROJECT_VERSION})
set_target_properties(pyignis PROPERTIES 
                      LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}/ignis"
                      RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/ignis"
                      DEBUG_POSTFIX "" # Disable suffix for this target
                      )
pybind11_extension(pyignis)
add_custom_command(TARGET pyignis POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/ignis "${OUTPUT_DIR}/ignis"
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml "${OUTPUT_DIR}/pyproject.toml"
                   COMMAND ${CMAKE_COMMAND} -E copy ${Ignis_SOURCE_DIR}/README.md "${OUTPUT_DIR}/README.md")
add_checks(pyignis)

set(SETUP_CFG ${OUTPUT_DIR}/setup.cfg)
configure_file(setup.cfg.in ${SETUP_CFG})

if(Python3_FOUND)
  set(_Python Python3)
else()
  set(_Python Python)
endif()

add_custom_target(pyignis-package
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml "${OUTPUT_DIR}/pyproject.toml"
    COMMAND ${CMAKE_COMMAND} -E copy ${Ignis_SOURCE_DIR}/README.md "${OUTPUT_DIR}/README.md"
    COMMAND ${${_Python}_EXECUTABLE} -m build
    DEPENDS ${SETUP_CFG} pyignis
    WORKING_DIRECTORY ${OUTPUT_DIR}
)