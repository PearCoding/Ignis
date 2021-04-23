# Setup actual driver
SET(CMD_FILES 
    main.cpp )

add_executable(ignis_trace ${CMD_FILES})
add_dependencies(ignis_trace ignis_drivers)
target_link_libraries(ignis_trace PRIVATE ig_lib_runtime)