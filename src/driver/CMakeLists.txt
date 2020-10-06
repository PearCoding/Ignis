#add_library(ig_lib_driver_base STATIC interface.cpp)

# TODO
SET(CMD_FILES main.cpp)
SET(UI_FILES main.cpp ui.cpp ui.h)

if(IG_HAS_UI)
    add_executable(ignis ${UI_FILES})
    target_compile_definitions(ignis PRIVATE "-DWITH_UI")
    target_link_libraries(ignis PRIVATE ig_lib_runtime ig_lib_ui)
else()
    add_executable(ignis ${CMD_FILES})
    target_link_libraries(ignis PRIVATE ig_lib_runtime)
endif()