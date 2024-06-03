# This directory should be first as it is exposing ARTIC_MAIN_SRC
add_subdirectory(artic)

add_subdirectory(internal/apicollector)

add_subdirectory(runtime)
add_subdirectory(device)
add_subdirectory(frontend)

if(IG_WITH_TOOLS)
add_subdirectory(tools)
endif()

if(BUILD_TESTING)
add_subdirectory(tests)
endif()