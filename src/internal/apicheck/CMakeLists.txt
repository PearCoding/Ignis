file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")
  
add_executable(ig_api_check ${SRC})
target_link_libraries(ig_api_check PUBLIC ig_lib_runtime)
target_compile_features(ig_api_check PUBLIC cxx_std_17)
add_checks(ig_api_check)
