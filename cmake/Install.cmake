# Setup cmake package
# TODO: This could be further optimized
include(CMakePackageConfigHelpers)

# CPack stuff
set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${Ignis_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR "${Ignis_VENDOR}")
set(CPACK_PACKAGE_DESCRIPTION "Experimental raytracer for the RENEGADE project implemented with the AnyDSL compiler framework for CPU and GPU.")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_CONTACT "https://github.com/PearCoding/Ignis")

SET(CPACK_COMPONENTS_ALL runtime tools documentation)
set(CPACK_COMPONENT_tools_DEPENDS runtime)

include(InstallArchive)
include(InstallBSD)
include(InstallDeb)
include(InstallRPM)

include(CPack)

cpack_add_component_group(runtime DISPLAY_NAME "Ignis runtime files")
cpack_add_component_group(viewer PARENT_GROUP runtime DISPLAY_NAME "Ignis tools")
cpack_add_component_group(documentation DISPLAY_NAME "Ignis documentation files")

cpack_add_component(runtime GROUP runtime)
cpack_add_component(tools GROUP tools)
cpack_add_component(documentation GROUP documentation)
