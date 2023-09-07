add_executable(obj2ply main.cpp)
target_link_libraries(obj2ply PRIVATE ig_lib_runtime)
install(TARGETS obj2ply ${_IG_RUNTIME_SET} COMPONENT tools)