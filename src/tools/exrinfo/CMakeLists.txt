add_executable(exrinfo main.cpp)
target_link_libraries(exrinfo PRIVATE ig_lib_runtime)
target_include_directories(exrinfo PRIVATE ${stb_SOURCE_DIR})
install(TARGETS exrinfo ${_IG_RUNTIME_SET} COMPONENT tools)