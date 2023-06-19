file(GLOB_RECURSE ARTIC_MAIN_SRC LIST_DIRECTORIES false *.art)
list(REMOVE_ITEM ARTIC_MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dummy_main.art ${CMAKE_CURRENT_SOURCE_DIR}/dummy_extra.art) # Do not include these by default

set(ARTIC_MAIN_SRC ${ARTIC_MAIN_SRC} PARENT_SCOPE)

anydsl_runtime_wrap(DISCARD_TMP_OBJS
    NAME "interface"
    FRONTEND "artic"
    FILES ${ARTIC_MAIN_SRC} dummy_main.art dummy_extra.art
    INTERFACE ${CMAKE_BINARY_DIR}/generated_interface)
add_custom_target(artic_c_interface ALL DEPENDS ${CMAKE_BINARY_DIR}/generated_interface.h)
