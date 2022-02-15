set(ARTIC_TEST_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/dummy_test.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bbox.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_common.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_intersection.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_matrix.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_microfacet.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reduction.art
)

# Compile artic stuff
SET(ARTIC_OBJS ) 
anydsl_runtime_wrap(ARTIC_OBJS
    NAME "artic_test"
    FRONTEND "artic"
    CLANG_FLAGS ${IG_ARTIC_CLANG_FLAGS}
    ARTIC_FLAGS ${IG_ARTIC_FLAGS} --log-level info -Wall
    FILES ${ARTIC_EXTRA_SRC} ${ARTIC_TEST_SRC}
    INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/generated_test_interface)

# Setup actual test
SET(_FILES 
    interface.cpp 
    main.cpp 
    ${ARTIC_OBJS}
    ${CMAKE_CURRENT_BINARY_DIR}/generated_test_interface.h)
    
add_executable(igtest ${_FILES})
target_link_libraries(igtest PRIVATE ${AnyDSL_runtime_LIBRARIES} ig_lib_runtime)
target_include_directories(igtest PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME ignis_test COMMAND igtest)
