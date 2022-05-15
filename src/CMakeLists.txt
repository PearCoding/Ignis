# This directory should be first as it is exposing ARTIC_MAIN_SRC and ARTIC_EXTRA_SRC
add_subdirectory(artic)

add_subdirectory(backend)
add_subdirectory(frontend)

if(IG_WITH_TOOLS)
add_subdirectory(tools)
endif()

if(BUILD_TESTING)
add_subdirectory(tests)
endif()