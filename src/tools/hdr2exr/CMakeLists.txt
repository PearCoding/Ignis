# Setup actual driver
SET(CMD_FILES 
    main.cpp )

add_executable(hdr2exr ${CMD_FILES})
target_link_libraries(hdr2exr PRIVATE ig_lib_runtime)
install(TARGETS hdr2exr COMPONENT tools)