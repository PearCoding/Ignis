# Setup actual driver
SET(CMD_FILES 
    main.cpp )
SET(UI_FILES 
    ${CMD_FILES}
    Pose.cpp
    Pose.h
    UI.cpp
    UI.h)

if(IG_HAS_UI)
    find_package(TBB REQUIRED) # Required for parallel stuff in UI.cpp
    add_executable(igview ${UI_FILES})
    target_compile_definitions(igview PRIVATE "-DWITH_UI")
    target_link_libraries(igview PRIVATE ig_lib_ui ig_lib_runtime TBB::tbb)
endif()

add_executable(igcli ${CMD_FILES})
target_link_libraries(igcli PRIVATE ig_lib_runtime )