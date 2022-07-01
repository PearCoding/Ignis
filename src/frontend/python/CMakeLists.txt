CPMAddPackage(
    NAME pybind11
    GITHUB_REPOSITORY pybind/pybind11 
    GIT_TAG stable
    EXCLUDE_FROM_ALL YES
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
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pyignis> "${OUTPUT_DIR}/ignis")
add_checks(pyignis)

set(SETUP_PY ${CMAKE_CURRENT_BINARY_DIR}/setup.py)
set(TIMESTAMP ${CMAKE_CURRENT_BINARY_DIR}/build/timestamp)
configure_file(setup.py.in ${SETUP_PY})

install(CODE "execute_process(COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} install --prefix ${CMAKE_INSTALL_PREFIX})" COMPONENT frontends)