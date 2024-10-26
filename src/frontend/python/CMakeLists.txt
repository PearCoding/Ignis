CPMAddPackage(
    NAME nanobind
    GITHUB_REPOSITORY wjakob/nanobind
    GIT_TAG v2.2.0
    EXCLUDE_FROM_ALL YES
    SYSTEM
)

set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/api")

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")
file(GLOB_RECURSE SRC_PY CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "ignis/*.py")

nanobind_add_module(pyignis STABLE_ABI LTO ${SRC})
# pybind11_add_module(pyignis MODULE ${SRC})
# add_library(pyignis MODULE ${SRC})

target_link_libraries(pyignis PRIVATE ig_runtime)
target_compile_definitions(pyignis PRIVATE IGNIS_VERSION=${PROJECT_VERSION})
set_target_properties(pyignis PROPERTIES 
                      LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}/ignis"
                      RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/ignis"
                      DEBUG_POSTFIX "" # Disable suffix for this target
                      )
ig_add_extra_options(pyignis)
add_custom_command(TARGET pyignis POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/ignis "${OUTPUT_DIR}/ignis"
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml "${OUTPUT_DIR}/pyproject.toml"
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${Ignis_SOURCE_DIR}/README.md "${OUTPUT_DIR}/README.md"
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${Ignis_SOURCE_DIR}/LICENSE.txt "${OUTPUT_DIR}/LICENSE"
                   DEPENDS ${SRC_PY})
# add_checks(pyignis)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ignis DESTINATION api/ COMPONENT python)
install(TARGETS pyignis ${_IG_RUNTIME_SET} COMPONENT python LIBRARY DESTINATION api/ignis RUNTIME DESTINATION api/ignis)

# Recursively call cmake from the toplevel again, this time only building the python api
# This is necessary as the project does not use python as the main setup tool,
# but only for packaging the final python api for pip and other install tools.
# Do not define the package target if we are already in the recursive call.
if(NOT SKBUILD)
  set(SETUP_CFG ${OUTPUT_DIR}/setup.cfg)
  set(SETUP_PY ${OUTPUT_DIR}/setup.py)
  configure_file(setup.cfg.in ${SETUP_CFG})
  configure_file(setup.py.in ${SETUP_PY})

  add_custom_target(pyignis-package
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml "${OUTPUT_DIR}/pyproject.toml"
      COMMAND ${CMAKE_COMMAND} -E copy ${Ignis_SOURCE_DIR}/README.md "${OUTPUT_DIR}/README.md"
      COMMAND ${CMAKE_COMMAND} -E copy ${Ignis_SOURCE_DIR}/LICENSE.txt "${OUTPUT_DIR}/LICENSE"
      COMMAND ${Python_EXECUTABLE} -m build 
      WORKING_DIRECTORY ${OUTPUT_DIR}
  )
endif()