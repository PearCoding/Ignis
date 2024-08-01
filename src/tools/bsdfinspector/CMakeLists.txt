if(NOT TARGET ig_ui)
return()
endif()

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(igbsdfinspector ${SRC})
target_link_libraries(igbsdfinspector PRIVATE ig_ui ig_common)
ig_add_extra_options(igbsdfinspector)
install(TARGETS igbsdfinspector ${_IG_RUNTIME_SET} COMPONENT tools)