file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(igtrace ${SRC})
target_link_libraries(igtrace PRIVATE ig_lib_common)
ig_add_extra_options(igtrace)
install(TARGETS igtrace ${_IG_RUNTIME_SET} COMPONENT frontends)
