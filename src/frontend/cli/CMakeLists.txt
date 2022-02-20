SET(SRC_FILES 
    main.cpp
    StatusObserver.h )

option(IG_DISABLE_EXECUTION_H "Disable the use of the <execution> header file" OFF)
mark_as_advanced(IG_DISABLE_EXECUTION_H)

add_executable(igcli ${SRC_FILES})
target_link_libraries(igcli PRIVATE ig_lib_common)
add_dependencies(igcli ignis_drivers)
install(TARGETS igcli COMPONENT runtime)

if(IG_DISABLE_EXECUTION_H)
    target_compile_definitions(igcli PRIVATE "-DIG_NO_EXECUTION_H")
endif()