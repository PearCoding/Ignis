# Setup actual driver
SET(CMD_FILES 
    main.cpp )

add_executable(ignis_trace ${CMD_FILES})
target_link_libraries(ignis_trace PRIVATE ignis_scene)