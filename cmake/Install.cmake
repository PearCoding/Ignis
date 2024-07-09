# Setup cmake package
include(CMakePackageConfigHelpers)

# CPack stuff
# set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT runtime)
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${Ignis_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR "${Ignis_VENDOR}")
set(CPACK_PACKAGE_DESCRIPTION "${Ignis_DESCRIPTION}")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_CONTACT "${Ignis_URL}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")

set(CPACK_PACKAGE_EXECUTABLES igexplorer "${PROJECT_NAME} Glare Viewer")
set(CPACK_CREATE_DESKTOP_LINKS igexplorer)

SET(CPACK_COMPONENTS_ALL runtime frontends plugins scenes)
if(TARGET ig_documentation)
    list(APPEND CPACK_COMPONENTS_ALL documentation)
endif()
if(IG_WITH_TOOLS)
    list(APPEND CPACK_COMPONENTS_ALL tools)
endif()
if(IG_HAS_PYTHON_API)
    list(APPEND CPACK_COMPONENTS_ALL python)
endif()

include(InstallArchive)
include(InstallBSD)
include(InstallDeb)
include(InstallNSIS)
include(InstallRPM)
include(InstallWIX)

include(CPack)

cpack_add_component(runtime DISPLAY_NAME "Runtime" DESCRIPTION "Necessary component containing runtime")
cpack_add_component(frontends DISPLAY_NAME "Frontends" DESCRIPTION "Frontends to interact with the runtime" DEPENDS runtime)
cpack_add_component(python DISPLAY_NAME "Python API" DESCRIPTION "Python 3+ API" DEPENDS runtime)
cpack_add_component(tools DISPLAY_NAME "Extra Tools" DESCRIPTION "Extra tools to work with data provided by the runtime" DEPENDS runtime)
cpack_add_component(documentation DISPLAY_NAME "Documentation" DESCRIPTION "Offline version of the documentation")
cpack_add_component(plugins DISPLAY_NAME "Plugins" DESCRIPTION "Plugin for external software like Blender")
cpack_add_component(scenes DISPLAY_NAME "Scenes" DESCRIPTION "Small scenes for Ignis for showcasing or evaluation purposes")

install(FILES "${CPACK_RESOURCE_FILE_LICENSE}" "${CPACK_RESOURCE_FILE_README}" TYPE DATA COMPONENT runtime)

if(IG_INSTALL_RUNTIME_DEPENDENCIES)
    if(WIN32)
        set(exclude_regexes "api-ms-" "ext-ms-")
    else()
        set(exclude_regexes 
            "libX.*" "libxkb.*" "libwayland.*" "libvorbis.*" "libexpat.*" "libFLAC.*" "libpulse.*" "libasound.*" # Skip media related libraries
            )
    endif()

    set(search_dirs "$<TARGET_FILE_DIR:ig_runtime>")
    if(IG_HAS_PYTHON_API)
        list(APPEND search_dirs "${Python_RUNTIME_LIBRARY_DIRS}")
        list(APPEND search_dirs "${Python_RUNTIME_SABI_LIBRARY_DIRS}")
    endif()

    find_package(CUDAToolkit)
    if(CUDAToolkit_FOUND)
        list(APPEND search_dirs "${CUDAToolkit_BIN_DIR}")
    endif()

    install(RUNTIME_DEPENDENCY_SET ignis_runtime_set
            PRE_EXCLUDE_REGEXES ${exclude_regexes}
            POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
            DIRECTORIES ${search_dirs}
            COMPONENT runtime
        )
endif()