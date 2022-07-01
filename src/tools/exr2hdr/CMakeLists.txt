# Setup actual driver
SET(CMD_FILES 
    main.cpp )

add_executable(exr2hdr ${CMD_FILES})
target_link_libraries(exr2hdr PRIVATE ig_lib_runtime)
target_include_directories(exr2hdr PRIVATE ${stb_SOURCE_DIR})
install(TARGETS exr2hdr ${_IG_RUNTIME_SET} COMPONENT tools)