SET(CMD_FILES 
    main.cpp
    IO.cpp
    IO.h
    Range.h 
    StatusObserver.h )
SET(UI_FILES 
    ${CMD_FILES}
    Pose.cpp
    Pose.h
    UI.cpp
    UI.h)

option(IG_DISABLE_EXECUTION_H "Disable the use of the <execution> header file" OFF)
mark_as_advanced(IG_DISABLE_EXECUTION_H)

if(IG_HAS_UI)
    add_executable(igview ${UI_FILES})
    target_compile_definitions(igview PRIVATE "-DWITH_UI")
    target_link_libraries(igview PRIVATE ig_lib_ui ig_lib_runtime)
    add_dependencies(igview ignis_drivers)

    if(IG_DISABLE_EXECUTION_H)
        target_compile_definitions(igview PRIVATE "-DIG_NO_EXECUTION_H")
    endif()
endif()

add_executable(igcli ${CMD_FILES})
target_link_libraries(igcli PRIVATE ig_lib_runtime)
add_dependencies(igcli ignis_drivers)

if(IG_DISABLE_EXECUTION_H)
    target_compile_definitions(igcli PRIVATE "-DIG_NO_EXECUTION_H")
endif()