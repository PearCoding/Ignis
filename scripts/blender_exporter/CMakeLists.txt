set(output ${CMAKE_BINARY_DIR}/ignis_blender.zip)
add_custom_target(ig_blender_plugin ALL
    COMMAND ${CMAKE_COMMAND} -E tar cf "${output}" --format=zip -- "${CMAKE_CURRENT_SOURCE_DIR}/ignis_blender"
    COMMENT "Creating blender plugin"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
install(FILES "${output}" DESTINATION plugins COMPONENT plugins)

find_program(BLENDER_EXECUTABLE NAMES blender)
if(BLENDER_EXECUTABLE)
    option(IG_INSTALL_BLENDER_PLUGIN "Allow the Blender plugin to be directly installed into Blender if available" OFF)
    if(IG_INSTALL_BLENDER_PLUGIN)
        add_custom_target(ig_blender_plugin_install ALL
            COMMAND ${BLENDER_EXECUTABLE} --command extension validate "${output}"
            COMMAND ${BLENDER_EXECUTABLE} --command extension install-file -e -r user_default "${output}"
            COMMENT "Installing blender plugin"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            DEPENDS ig_blender_plugin
        )
    endif()
endif()
