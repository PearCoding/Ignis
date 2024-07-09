file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(igview ${SRC})
target_link_libraries(igview PRIVATE ig_ui ig_common)
ig_add_extra_options(igview)
install(TARGETS igview ${_IG_RUNTIME_SET} COMPONENT frontends)
