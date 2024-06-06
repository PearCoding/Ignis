# install(DIRECTORY . DESTINATION scenes COMPONENT scenes
#     PATTERN ignis_cache* EXCLUDE
#     PATTERN blender EXCLUDE
#     PATTERN mitsuba EXCLUDE
#     PATTERN *.blend EXCLUDE
#     PATTERN *.sh EXCLUDE
# )

# Install a simple scene and not everything as this results in quite a large installer
set(SCENES_TO_INSTALL
    diamond_scene.json
)

set(MESHES_TO_INSTALL
    meshes/Bottom.ply
    meshes/Top.ply
    meshes/Left.ply
    meshes/Right.ply
    meshes/Back.ply
    meshes/Diamond.ply
)

install(FILES ${SCENES_TO_INSTALL} DESTINATION scenes COMPONENT scenes)
install(FILES ${MESHES_TO_INSTALL} DESTINATION scenes/meshes COMPONENT scenes)

if(WIN32)
    install(FILES ../scripts/run_example.ps1 DESTINATION / COMPONENT scenes)
endif()
