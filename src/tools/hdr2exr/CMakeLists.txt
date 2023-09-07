add_executable(hdr2exr main.cpp)
target_link_libraries(hdr2exr PRIVATE ig_lib_runtime)
install(TARGETS hdr2exr ${_IG_RUNTIME_SET} COMPONENT tools)