SET(SRC_FILES 
    main.cpp )

add_executable(ig_test_fuzzer ${SRC_FILES})
target_link_libraries(ig_test_fuzzer PRIVATE ig_lib_runtime)
ig_add_extra_options(ig_test_fuzzer)

set(SANITIZER_FLAGS -fsanitize=address -g -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -fsanitize=memory -fsanitize=undefined)
check_cxx_compiler_flag("${SANITIZER_FLAGS}" COMPILER_SUPPORTS_SANITIZER)
if(COMPILER_SUPPORTS_SANITIZER)
    message(STATUS "Fuzzer is using additional compiler flags: ${SANITIZER_FLAGS}")
    target_compile_options(ig_test_fuzzer PRIVATE ${SANITIZER_FLAGS})
    target_link_options(ig_test_fuzzer PRIVATE ${SANITIZER_FLAGS})
endif()

# add_test(NAME ignis_test_fuzzer COMMAND ig_test_fuzzer 1000)
