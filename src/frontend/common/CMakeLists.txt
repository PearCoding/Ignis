
add_library(ig_lib_common STATIC IO.h IO.cpp)
target_link_libraries(ig_lib_common PUBLIC ig_lib_runtime)
target_include_directories(ig_lib_common PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
