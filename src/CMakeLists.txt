# This directory should be first as it is exposing ARTIC_SRC
add_subdirectory(artic)

add_subdirectory(backend)
add_subdirectory(frontend)

if(IG_WITH_TOOLS)
add_subdirectory(tools)
endif()

add_subdirectory(tests)