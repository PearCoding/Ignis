find_package(Sphinx)

if(NOT Sphinx_FOUND)
    return()
endif()

set(SPHINX_INPUT_DIR  "${CMAKE_CURRENT_SOURCE_DIR}")
set(SPHINX_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/html")

add_custom_target(ig_documentation
    COMMAND ${SPHINX_EXECUTABLE} -M html "${SPHINX_INPUT_DIR}" "${SPHINX_OUTPUT_DIR}"
    COMMENT "Building HTML documentation with Sphinx"
    USES_TERMINAL
)

set(SPHINX_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

add_custom_target(ig_documentation_pdf
    COMMAND ${SPHINX_EXECUTABLE} -M latexpdf "${SPHINX_INPUT_DIR}" "${SPHINX_OUTPUT_DIR}"
    COMMENT "Building LaTeX PDF documentation with Sphinx"
    USES_TERMINAL
)