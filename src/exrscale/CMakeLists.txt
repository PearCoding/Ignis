# Setup actual driver
SET(CMD_FILES 
    main.cpp )

add_executable(exrscale ${CMD_FILES})
target_link_libraries(exrscale PRIVATE OpenImageIO::OpenImageIO OpenImageIO::OpenImageIO_Util)