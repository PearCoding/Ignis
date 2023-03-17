
set(output ${CMAKE_BINARY_DIR}/ignis_blender.zip)
add_custom_target(ig_blender_plugin ALL
    COMMAND ${CMAKE_COMMAND} -E tar cf "${output}" --format=zip -- "${CMAKE_CURRENT_SOURCE_DIR}/ignis_blender"
    COMMENT "Creating blender plugin"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
install(FILES "${output}" DESTINATION plugins COMPONENT plugins)
