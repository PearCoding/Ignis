# Setup actual driver
SET(CMD_FILES 
    main.cpp )

add_executable(exr2jpg ${CMD_FILES})
target_link_libraries(exr2jpg PRIVATE ig_lib_runtime)
target_include_directories(exr2jpg PRIVATE ${stb_SOURCE_DIR})
install(TARGETS exr2jpg COMPONENT tools)