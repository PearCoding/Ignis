# Setup actual driver
SET(CMD_FILES 
    main.cpp )

add_executable(exr2hdr ${CMD_FILES})
target_link_libraries(exr2hdr PRIVATE OpenImageIO::OpenImageIO)