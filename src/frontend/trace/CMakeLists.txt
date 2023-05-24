SET(SRC_FILES 
    main.cpp )

add_executable(igtrace ${SRC_FILES})
target_link_libraries(igtrace PRIVATE ig_lib_common)
add_lto(igtrace)
add_checks(igtrace)
add_linker_options(igtrace)
install(TARGETS igtrace ${_IG_RUNTIME_SET} COMPONENT frontends)
