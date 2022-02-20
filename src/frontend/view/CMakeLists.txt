SET(SRC_FILES 
    main.cpp
    Pose.cpp
    Pose.h
    UI.cpp
    UI.h)

add_executable(igview ${SRC_FILES})
target_compile_definitions(igview PRIVATE "-DWITH_UI")
target_link_libraries(igview PRIVATE ig_lib_ui ig_lib_common)
add_dependencies(igview ignis_drivers)
install(TARGETS igview COMPONENT runtime)
