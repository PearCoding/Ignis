CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2 
    GIT_TAG v3.7.1
    EXCLUDE_FROM_ALL YES
    SYSTEM
)
    
macro(push_test name files)
	set(options NO_ADD)
	set(oneValueArgs)
	set(multiValueArgs)
	cmake_parse_arguments(__args "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

	set(TARGET ig_test_${name})
	add_executable(${TARGET} ${files})
	target_link_libraries(${TARGET} PRIVATE ig_runtime Catch2WithMain)
    ig_add_extra_options(${TARGET} NO_LTO)
	if(NOT __args_NO_ADD)
		add_test(NAME ${name} COMMAND ${TARGET})
	endif()
endmacro(push_test)

push_test(elevation_azimuth elevation_azimuth.cpp)
push_test(perez perez.cpp)
push_test(sun sun.cpp)
push_test(trimesh_plane trimesh_plane.cpp)
push_test(trimesh_sphere trimesh_sphere.cpp)
push_test(trimesh_he trimesh_he.cpp)
