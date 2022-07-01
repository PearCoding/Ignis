# Setup cmake package
# TODO: This could be further optimized
include(CMakePackageConfigHelpers)

# CPack stuff
set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${Ignis_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR "${Ignis_VENDOR}")
set(CPACK_PACKAGE_DESCRIPTION "${Ignis_DESCRIPTION}")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_CONTACT "${Ignis_URL}")

SET(CPACK_COMPONENTS_ALL runtime frontends documentation)
set(CPACK_COMPONENT_frontends_DEPENDS runtime)

include(InstallArchive)
include(InstallBSD)
include(InstallDeb)
include(InstallRPM)

include(CPack)

cpack_add_component_group(runtime DISPLAY_NAME "Ignis runtime files")
cpack_add_component_group(frontends PARENT_GROUP runtime DISPLAY_NAME "Ignis frontends")
cpack_add_component_group(tools PARENT_GROUP runtime DISPLAY_NAME "Ignis extra tools")
cpack_add_component_group(documentation DISPLAY_NAME "Ignis documentation files")

cpack_add_component(runtime GROUP runtime)
cpack_add_component(frontends GROUP frontends)
cpack_add_component(tools GROUP tools)
cpack_add_component(documentation GROUP documentation)

if(IG_INSTALL_RUNTIME_DEPENDENCIES)
    if(WIN32)
        set(exclude_regexes "msvc.*" "api.*" "ext.*")
    else()
        set(exclude_regexes "libX.*" "libxkb.*" "libwayland.*" "libvorbis.*" "libexpat.*" "libFLAC.*" "libpulse.*" "libasound.*" )
    endif()
    install(RUNTIME_DEPENDENCY_SET ignis_runtime_set
            PRE_EXCLUDE_REGEXES ${exclude_regexes}
        )
endif()