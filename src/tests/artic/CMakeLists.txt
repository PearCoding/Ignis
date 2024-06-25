file(GLOB_RECURSE ARTIC_TEST_SRC CONFIGURE_DEPENDS "*.art")

# Compile artic stuff
SET(ARTIC_OBJS ) 
anydsl_runtime_wrap(ARTIC_OBJS
    NAME "artic_test"
    FRONTEND "artic"
    CLANG_FLAGS ${IG_ARTIC_CLANG_FLAGS}
    ARTIC_FLAGS ${IG_ARTIC_FLAGS} --log-level info -Wall
    FILES ${ARTIC_MAIN_SRC} ${ARTIC_TEST_SRC}
    INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/generated_test_interface)

# Setup actual test
SET(_FILES 
    interface.cpp 
    main.cpp 
    ${ARTIC_OBJS}
    ${CMAKE_CURRENT_BINARY_DIR}/generated_test_interface.h)
    
add_executable(ig_test_artic ${_FILES})
target_link_libraries(ig_test_artic PRIVATE ig_lib_runtime ${AnyDSL_runtime_LIBRARIES})
target_include_directories(ig_test_artic PRIVATE ${CMAKE_CURRENT_BINARY_DIR})