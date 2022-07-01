SET(SRC_FILES 
    main.cpp
    StatusObserver.h )

add_executable(igcli ${SRC_FILES})
target_link_libraries(igcli PRIVATE ig_lib_common)
add_dependencies(igcli ignis_drivers)
add_lto(igcli)
add_checks(igcli)
install(TARGETS igcli ${_IG_RUNTIME_SET} COMPONENT frontends)
