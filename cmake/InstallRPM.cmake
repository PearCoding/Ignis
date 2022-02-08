# RPM Stuff
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_MAIN_COMPONENT runtime)

set(CPACK_RPM_runtime_PACKAGE_DESCRIPTION "Runtime environment for the ${PROJECT_NAME} raytracer.")

set(CPACK_RPM_tools_PACKAGE_DESCRIPTION "Tools for the ${PROJECT_NAME} raytracer.")

set(CPACK_RPM_documentation_PACKAGE_DESCRIPTION "API Documentation for the ${PROJECT_NAME} raytracer.")
