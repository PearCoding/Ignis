# Setup actual driver
SET(CMD_FILES 
    main.cpp )

add_executable(hdr2exr ${CMD_FILES})
target_link_libraries(hdr2exr PRIVATE OpenImageIO::OpenImageIO OpenImageIO::OpenImageIO_Util)