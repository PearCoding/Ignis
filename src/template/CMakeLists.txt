CPMAddPackage(
    NAME stpp
    GITHUB_REPOSITORY PearCoding/stpp 
    GIT_TAG main
    EXCLUDE_FROM_ALL
)

# TODO: Make it configurable
set(CAMERAS
    "-D CAMERA_PERSPECTIVE"
    #"-D CAMERA_ORTHOGONAL"
    #"-D CAMERA_FISHEYE"
    "-D CAMERA_LIST")

set(CAMERA_NAMES
    "Perspective"
    #"Orthogonal"
    #"Fisheye"
    "List")

set(DEVICES
    "-D DEVICE_DEFAULT"
    #"-D DEVICE_AVX"
    "-D DEVICE_AVX2"
    "-D DEVICE_SSE42"
    "-D DEVICE_ASIMD"
    "-D DEVICE_NVVM"
    #"-D DEVICE_NVVM_MEGA"
    #"-D DEVICE_AMD"
    #"-D DEVICE_AMD_MEGA"
    )

set(DEVICE_NAMES
    "Default"
    #"AVX"
    "AVX2"
    "SSE42"
    "ASIMD"
    "NVVM"
    #"NVVM_MEGA"
    #"AMD"
    #"AMD_MEGA"
    )

set(RENDERER
    "-D RENDERER_PATH"
    "-D RENDERER_DEBUG")

set(RENDERER_NAMES
    "Path"
    "Debug")

function(cross out_list list1 list2 sep)
    set(_list )
    foreach(_var1 ${${list1}})
        foreach(_var2 ${${list2}})
        list(APPEND _list "${_var1}${sep}${_var2}")
        endforeach()
    endforeach()
    set(${out_list} ${_list} PARENT_SCOPE)
endfunction()

cross(VARIANTS_1 CAMERAS DEVICES " ")
cross(VARIANTS_2 VARIANTS_1 RENDERER " ")

cross(VARIANT_NAMES_1 CAMERA_NAMES DEVICE_NAMES "_")
cross(VARIANT_NAMES_2 VARIANT_NAMES_1 RENDERER_NAMES "_")

set(VARIANTS ${VARIANTS_2})
set(VARIANT_NAMES ${VARIANT_NAMES_2})

set(_counter 0)
set(_targets )
foreach(var ${VARIANTS})
    list(GET VARIANT_NAMES ${_counter} var_name)
    message(STATUS "${var_name} = ${var}")
    set(out_file "${CMAKE_CURRENT_BINARY_DIR}/main_${var_name}.art")
    separate_arguments(args UNIX_COMMAND ${var})
    add_custom_command(
        OUTPUT "${out_file}"
        COMMAND $<TARGET_FILE:stpp> ${args} ${CMAKE_CURRENT_SOURCE_DIR}/main.artpp > ${out_file}
        MAIN_DEPENDENCY main.artpp
        COMMENT "Generating variant file: ${out_file}" 
        VERBATIM)
    set(_objs ) # Reset list
    anydsl_runtime_wrap(_objs
        NAME "artic_${var_name}"
        FRONTEND "artic"
        CLANG_FLAGS ${ARTIC_CLANG_FLAGS}
        ARTIC_FLAGS --log-level info --max-errors 5
        FILES ${ARTIC_SRC} ${out_file})
    set(_target_name ignis_driver_${var_name})
    add_library(${_target_name} MODULE ${_objs})
    set_target_properties(${_target_name} PROPERTIES PREFIX "")
    list(APPEND _targets ${_target_name})
    math(EXPR _counter "${_counter}+1")
endforeach()

add_custom_target(ignis_drivers)
add_dependencies(ignis_drivers ${_targets})
