SET(SRC_FILES 
    main.cpp
    StatusObserver.h )

add_executable(igcli ${SRC_FILES})
target_link_libraries(igcli PRIVATE ig_lib_common)
add_dependencies(igcli ignis_drivers)
add_lto(igcli)
install(TARGETS igcli COMPONENT runtime)
