add_executable(ig_profiler main.cpp UI.cpp UI.h)
target_link_libraries(ig_profiler PRIVATE ig_lib_ui ig_lib_runtime)
install(TARGETS ig_profiler ${_IG_RUNTIME_SET} COMPONENT tools)