add_executable(ply2obj main.cpp)
target_link_libraries(ply2obj PRIVATE ig_lib_runtime)
install(TARGETS ply2obj ${_IG_RUNTIME_SET} COMPONENT tools)