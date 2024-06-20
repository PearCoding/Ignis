file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(igcli ${SRC})
target_link_libraries(igcli PRIVATE ig_lib_common)
add_lto(igcli)
add_checks(igcli)
add_linker_options(igcli)
install(TARGETS igcli ${_IG_RUNTIME_SET} COMPONENT frontends)
