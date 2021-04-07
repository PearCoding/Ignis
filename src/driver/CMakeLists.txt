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
    add_executable(ignis ${UI_FILES})
    target_compile_definitions(ignis PRIVATE "-DWITH_UI")
    target_link_libraries(ignis PRIVATE ig_lib_ui TBB::tbb)
    target_link_libraries(ignis PRIVATE ignis_scene)
endif()

add_executable(ignis_cli ${CMD_FILES})
target_link_libraries(ignis_cli PRIVATE ignis_scene)