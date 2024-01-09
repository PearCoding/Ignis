set(ARTIC_TEST_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/dummy_test.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bbox.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cdf.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_common.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_intersection.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_interval.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_matrix.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_microfacet.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reduction.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_warp.art
    ${CMAKE_CURRENT_SOURCE_DIR}/test_neural.art
)

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
target_link_libraries(ig_test_artic PRIVATE ${AnyDSL_runtime_LIBRARIES} ig_lib_runtime)
target_include_directories(ig_test_artic PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
if(AnyDSL_runtime_HAS_CUDA_SUPPORT)
    add_test(NAME ignis_test_artic COMMAND ig_test_artic)
else()
    add_test(NAME ignis_test_artic COMMAND ig_test_artic --no-gpu)
endif()