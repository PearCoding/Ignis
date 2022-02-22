# Setup actual driver
SET(CMD_FILES 
    main.cpp )

add_executable(igtrace ${CMD_FILES})
add_dependencies(igtrace ignis_drivers)
target_link_libraries(igtrace PRIVATE ig_lib_common)
install(TARGETS igtrace COMPONENT runtime)