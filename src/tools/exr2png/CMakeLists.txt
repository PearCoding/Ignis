add_executable(exr2png main.cpp)
target_link_libraries(exr2png PRIVATE ig_lib_runtime)
target_include_directories(exr2png PRIVATE ${stb_SOURCE_DIR})
install(TARGETS exr2png ${_IG_RUNTIME_SET} COMPONENT tools)