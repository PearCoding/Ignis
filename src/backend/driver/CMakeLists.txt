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
    #"-D CAMERA_FISHLENS"
    #"-D CAMERA_LIST"
)

# It is more useful (and user friendly) to choose between cpu and gpu
# Each of the two groups selects the respective best architecture (e.g., AVX2 for CPU and NVVM for GPU)
# This would require a prepass to generate a list of targets to generate
# Multi vendor multi GPU and CPU combinations would be bothersome, but who has this kind of configuration?
set(DEVICES
    #"-D DEVICE_DEFAULT"
    #"-D DEVICE_AVX"
    "-D DEVICE_AVX2"
    #"-D DEVICE_AVX512"  # [NOT_TESTED, as not supported by my CPU]
    #"-D DEVICE_SSE42"
    #"-D DEVICE_ASIMD"
    "-D DEVICE_NVVM"
    "-D DEVICE_NVVM_MEGA"
    #"-D DEVICE_AMD"
    #"-D DEVICE_AMD_MEGA"
)

set(RENDERER
    "-D RENDERER_PATH"
    #"-D RENDERER_DEBUG"
    #"-D RENDERER_AO"
)

#############################################

function(extract_name out_var in_var)
    string(REGEX REPLACE "^([a-zA-Z]+_)(.+)$" "\\2" _t1 ${in_var})
    string(TOLOWER ${_t1} _t2)
    set(${out_var} ${_t2} PARENT_SCOPE)
endfunction()

function(var_to_name out_var variant)
    string(REPLACE " " ";" _l1 "${variant}")
    set(_names )
    foreach(_var1 ${_l1})
        if(NOT "${_var1}" STREQUAL "-D")
            extract_name(_n1 ${_var1})
            list(APPEND _names "${_n1}")
        endif()
    endforeach()
    string(JOIN "_" _names2 ${_names})
    set(${out_var} ${_names2} PARENT_SCOPE)
endfunction()

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

set(VARIANTS ${VARIANTS_2})

option(IG_BUILD_DRIVER_PARALLEL "Build driver files in parallel. Not recommended" OFF)

set(_targets )
foreach(var ${VARIANTS})
    var_to_name(var_name ${var})
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
    set(_interface_name driver_interface_${var_name})
    anydsl_runtime_wrap(_objs
        NAME "artic_${var_name}"
        FRONTEND "artic"
        CLANG_FLAGS ${ARTIC_CLANG_FLAGS}
        ARTIC_FLAGS --log-level info -Wall
        FILES ${ARTIC_SRC} ${out_file})
    set(_target_name ig_driver_${var_name})
    add_library(${_target_name} MODULE ${_objs} glue.cpp)
    set_target_properties(${_target_name} PROPERTIES PREFIX "")
    target_compile_definitions(${_target_name} PRIVATE ${args})
    target_link_libraries(${_target_name} PRIVATE ${AnyDSL_runtime_LIBRARIES} ig_lib_runtime)
    target_include_directories(${_target_name} PUBLIC $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>)
    add_dependencies(${_target_name} artic_c_interface)

    # Some of the compilation process requires a lot of memory.
    # Prevent parallel build of the drivers (per default) by chaining them together
    # This will rebuild all the drivers up the line,
    # however drivers are dynamicly defined and ment to be rebuild anyway.
    if(NOT IG_BUILD_DRIVER_PARALLEL)
        list(LENGTH _targets _length)
        if(_length GREATER 0)
            list(GET _targets -1 _prev)
            add_dependencies(${_target_name} ${_prev})
        endif()
    endif()

    list(APPEND _targets ${_target_name})
endforeach()

add_custom_target(ignis_drivers)
add_dependencies(ignis_drivers ${_targets})
