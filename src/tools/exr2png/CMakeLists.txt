# Setup actual driver
SET(CMD_FILES 
    main.cpp )

add_executable(exr2png ${CMD_FILES})
target_link_libraries(exr2png PRIVATE ig_lib_runtime)
target_include_directories(exr2png PRIVATE ${stb_SOURCE_DIR})