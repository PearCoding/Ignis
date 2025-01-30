file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(igevalglare ${SRC})
target_link_libraries(igevalglare PRIVATE ig_common)
ig_add_extra_options(igevalglare)
install(TARGETS igevalglare ${_IG_RUNTIME_SET} COMPONENT tools)
