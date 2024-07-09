file(GLOB_RECURSE ARTIC_TEST_SRC CONFIGURE_DEPENDS "*.art")
set(ARTIC_TEST_FILE ${CMAKE_CURRENT_BINARY_DIR}/tests.cpp)

add_custom_command(
  OUTPUT ${ARTIC_TEST_FILE}
  COMMAND $<TARGET_FILE:ig_api_collector> --prefix ig_test_shader ${ARTIC_TEST_SRC} ${ARTIC_TEST_FILE}
  DEPENDS ${ARTIC_TEST_SRC}
  VERBATIM)

# Setup actual test
SET(_FILES 
    interface.cpp 
    main.cpp 
    ${ARTIC_TEST_FILE})
    
add_executable(ig_test_artic ${_FILES})
target_link_libraries(ig_test_artic PRIVATE ig_runtime)
ig_add_extra_options(ig_test_artic NO_LTO)