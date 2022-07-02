CPMAddPackage(
    NAME pybind11
    GITHUB_REPOSITORY pybind/pybind11 
    GIT_TAG stable
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "PYBIND11_FINDPYTHON ON"
)

set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/api")

pybind11_add_module(pyignis api.cpp)
add_dependencies(pyignis ignis_drivers)
target_link_libraries(pyignis PRIVATE ig_lib_runtime)
target_compile_definitions(pyignis PRIVATE IGNIS_VERSION=${PROJECT_VERSION})
set_target_properties(pyignis PROPERTIES 
                      DEBUG_POSTFIX "" # Disable suffix for this target
                      )
add_custom_command(TARGET pyignis POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/ignis "${OUTPUT_DIR}/ignis"
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pyignis> "${OUTPUT_DIR}/ignis"
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