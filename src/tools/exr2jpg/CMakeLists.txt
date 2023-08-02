add_executable(exr2jpg main.cpp)
target_link_libraries(exr2jpg PRIVATE ig_lib_runtime)
target_include_directories(exr2jpg PRIVATE ${stb_SOURCE_DIR})
install(TARGETS exr2jpg ${_IG_RUNTIME_SET} COMPONENT tools)