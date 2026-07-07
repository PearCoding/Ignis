SET(SRC_FILES 
    main.cpp )

add_executable(ig_test_fuzzer ${SRC_FILES})
target_link_libraries(ig_test_fuzzer PRIVATE ig_runtime)
ig_add_extra_options(ig_test_fuzzer)

# AddressSanitizer and MemorySanitizer are mutually exclusive, so requesting both made the
# whole flag check fail and no sanitizer was ever applied. Use ASan + UBSan.
set(SANITIZER_FLAGS -fsanitize=address -g -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -fsanitize=undefined)
check_cxx_compiler_flag("${SANITIZER_FLAGS}" COMPILER_SUPPORTS_SANITIZER)
if(COMPILER_SUPPORTS_SANITIZER)
    message(STATUS "Fuzzer is using additional compiler flags: ${SANITIZER_FLAGS}")
    target_compile_options(ig_test_fuzzer PRIVATE ${SANITIZER_FLAGS})
    target_link_options(ig_test_fuzzer PRIVATE ${SANITIZER_FLAGS})
endif()

# The fuzzer runs many randomized iterations and is slow, so it is kept out of the
# automatic ctest run by default. The ig_test_fuzzer binary is always built and can
# be run directly; enable IG_WITH_FUZZER_TEST to also register it as a ctest test.
option(IG_WITH_FUZZER_TEST "Register the slow fuzzer as an automatic ctest test" OFF)
if(IG_WITH_FUZZER_TEST)
    add_test(NAME ignis_test_fuzzer COMMAND ig_test_fuzzer 1000)
    set_tests_properties(ignis_test_fuzzer PROPERTIES LABELS "cpu")
endif()
