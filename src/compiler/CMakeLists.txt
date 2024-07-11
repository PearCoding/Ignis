file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(igc ${SRC})
target_link_libraries(igc PRIVATE ig_common)
ig_add_extra_options(igc)
install(TARGETS igc ${_IG_RUNTIME_SET} COMPONENT runtime)
