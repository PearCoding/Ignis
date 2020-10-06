
add_subdirectory(hardware)
add_subdirectory(runtime)
add_subdirectory(driver)

if(IG_HAS_UI)
    add_subdirectory(ui)
endif()