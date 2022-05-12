set(IG_DISABLE_DEVICES "" CACHE STRING "Devices to not build a driver for.")
mark_as_advanced(IG_DISABLE_DEVICES)

string(REPLACE " " ";" _DISABLED_DEVICES "${IG_DISABLE_DEVICES}")
list(REMOVE_DUPLICATES _DISABLED_DEVICES)
list(TRANSFORM _DISABLED_DEVICES TOUPPER)

###########################################################
option(IG_ASSUME_NVIDIA "Assume CUDA libraries are available" OFF)
mark_as_advanced(IG_ASSUME_NVIDIA)
if(IG_ASSUME_NVIDIA OR AnyDSL_runtime_HAS_CUDA_SUPPORT)
    set(_HAS_CUDA ON)
else()
    find_library(CUDA_LIBRARY cudart HINTS ${CUDA_DIR} PATHS /usr/local/cuda
    ENV CUDA_PATH
    ENV CUDA_LIB_PATH
    PATH_SUFFIXES "lib/Win32" "lib" "libWin32")
    if(EXISTS "${CUDA_LIBRARY}")
        set(_HAS_CUDA ON)
    endif()
endif()

###########################################################
option(IG_ASSUME_AMD "Assume AMD rocm libraries are available" OFF)
mark_as_advanced(IG_ASSUME_AMD)
if(IG_ASSUME_AMD OR AnyDSL_runtime_HAS_HSA_SUPPORT)
    set(_HAS_HSA ON)
else()
    find_library(HSA_LIBRARY hsa-runtime64 HINTS ${HSA_DIR} PATHS /opt/rocm PATH_SUFFIXES lib)
    if(EXISTS "${HSA_LIBRARY}")
        set(_HAS_HSA ON)
    endif()
endif()

###########################################################
include(CMakeDependentOption)
option(IG_WITH_DEVICE_CPU "Enable CPU device with optional vectorization support" ON)
option(IG_WITH_DEVICE_GPU "Enable GPU device for NVidia or AMD GPUs"              ON)

cmake_dependent_option(IG_WITH_DEVICE_CPU_SIMD   "Enable CPU vectorized device" ON  "IG_WITH_DEVICE_CPU"           OFF)
cmake_dependent_option(IG_WITH_DEVICE_GPU_NVIDIA "Enable NVidia GPU device"     ON  "IG_WITH_DEVICE_GPU;_HAS_CUDA" OFF)
cmake_dependent_option(IG_WITH_DEVICE_GPU_AMD    "Enable AMD GPU device"        ON  "IG_WITH_DEVICE_GPU;_HAS_HSA"  OFF)

cmake_dependent_option(IG_WITH_DEVICE_CPU_SSE42          "Force SSE 4.2 driver"                OFF  "IG_WITH_DEVICE_CPU_SIMD" OFF)
cmake_dependent_option(IG_WITH_DEVICE_CPU_AVX            "Force AVX driver"                    OFF  "IG_WITH_DEVICE_CPU_SIMD" OFF)
cmake_dependent_option(IG_WITH_DEVICE_CPU_AVX2           "Force AVX 2 driver"                  OFF  "IG_WITH_DEVICE_CPU_SIMD" OFF)
cmake_dependent_option(IG_WITH_DEVICE_CPU_AVX512         "Force AVX 512 driver"                OFF  "IG_WITH_DEVICE_CPU_SIMD" OFF)
cmake_dependent_option(IG_WITH_DEVICE_CPU_BEST           "Select the best instruction set"     ON   "IG_WITH_DEVICE_CPU_SIMD" OFF)
cmake_dependent_option(IG_WITH_DEVICE_CPU_GENERIC_ALWAYS "Always build the generic CPU driver" OFF  "IG_WITH_DEVICE_CPU"      OFF)

cmake_dependent_option(IG_WITH_DEVICE_CPU_ST_GENERIC "Enable the single threaded generic CPU driver. This is only useful for debug purposes" OFF  "IG_WITH_DEVICE_CPU" OFF)

set(DEVICE_SELECTION )

if(IG_WITH_DEVICE_CPU)
    if(IG_WITH_DEVICE_CPU_SIMD)
        if(IG_WITH_DEVICE_CPU_BEST)
            # TODO: This only works for x64
            set(_code "#include <immintrin.h>
            __m512i i;
            int main(void) {__m521i a = _mm512_abs_epi32(i); return 0;}")
            check_cxx_source_compiles("${_code}" HAS_AVX512)

            if(HAS_AVX512)
                set(BEST "AVX512")
            else()
                set(_code "#include <immintrin.h>
                __m256i i;
                int main(void) {__m256i a = _mm256_abs_epi16(i); return 0;}")
                check_cxx_source_compiles("${_code}" HAS_AVX2)
                
                if(HAS_AVX2)
                    set(BEST "AVX2")
                else()
                    set(_code "#include <immintrin.h>
                    int main(void) {__m256 a = _mm256_setzero_ps(); return 0;}")
                    check_cxx_source_compiles("${_code}" HAS_AVX)

                    if(HAS_AVX)
                        set(BEST "AVX")
                    else()
                        set(_code "#include <nmmintrin.h>
                        unsigned int i;
                        int main(void) {unsigned int a = _mm_crc32_u32(0, i); return 0;}")

                        check_cxx_source_compiles("${_code}" HAS_SSE42)
                        if(HAS_SSE42)
                            set(BEST "SSE42")
                        endif()
                    endif()
                endif()
            endif()
        else()
            set(BEST )
        endif()

        if(IG_WITH_DEVICE_CPU_SSE42 OR BEST STREQUAL "SSE42" )
            list(APPEND DEVICE_SELECTION "SSE42")
        endif()
        
        if(IG_WITH_DEVICE_CPU_AVX OR BEST STREQUAL "AVX" )
            list(APPEND DEVICE_SELECTION "AVX")
        endif()
        
        if(IG_WITH_DEVICE_CPU_AVX2 OR BEST STREQUAL "AVX2" )
            list(APPEND DEVICE_SELECTION "AVX2")
        endif()
        
        if(IG_WITH_DEVICE_CPU_AVX512 OR BEST STREQUAL "AVX512" )
            list(APPEND DEVICE_SELECTION "AVX512")
        endif()

        if(NOT DEVICE_SELECTION)
            list(APPEND DEVICE_SELECTION "ASIMD")
        endif()
    else()
        list(APPEND DEVICE_SELECTION "DEFAULT")
    endif()

    if(IG_WITH_DEVICE_CPU_GENERIC_ALWAYS AND IG_WITH_DEVICE_CPU_SIMD)
        list(APPEND DEVICE_SELECTION "DEFAULT")
    endif()

    if(IG_WITH_DEVICE_CPU_ST_GENERIC)
        list(APPEND DEVICE_SELECTION "SINGLE")
    endif()
endif()

if(IG_WITH_DEVICE_GPU)
    if(IG_WITH_DEVICE_GPU_NVIDIA)
        list(APPEND DEVICE_SELECTION "NVVM")
    endif()
    if(IG_WITH_DEVICE_GPU_AMD)
        list(APPEND DEVICE_SELECTION "AMDGPU")
    endif()
endif()

if(NOT DEVICE_SELECTION)
    message(FATAL_ERROR "No devices selected. Aborting")
endif()

message(STATUS "Selected devices:  ${DEVICE_SELECTION}")
message(STATUS "Artic Clang Flags: ${IG_ARTIC_CLANG_FLAGS}")
message(STATUS "Artic Flags:       ${IG_ARTIC_FLAGS}")

set(DEVICES )
foreach(dev IN LISTS DEVICE_SELECTION)
    if(NOT "${dev}" IN_LIST _DISABLED_DEVICES)
        list(APPEND DEVICES "-D DEVICE_${dev}")
    endif()
endforeach()

if(NOT DEVICES)
    message(FATAL_ERROR "All devices are filtered out. Aborting")
endif()

###########################################################
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

set(VARIANTS ${DEVICES})

if(NOT VARIANTS)
    message(FATAL_ERROR "No variants available. This error should have been catched earlier... Aborting")
endif()

###########################################################
option(IG_BUILD_DRIVER_PARALLEL "Build driver files in parallel. Not recommended" OFF)

set(_targets )
foreach(var ${VARIANTS})
    var_to_name(var_name ${var})
    message(VERBOSE "Driver: ${var_name} = ${var}")

    # Create driver specific code
    set(out_file "${CMAKE_CURRENT_BINARY_DIR}/driver_${var_name}.art")
    separate_arguments(args UNIX_COMMAND ${var})
    add_custom_command(
        OUTPUT ${out_file}
        COMMAND $<TARGET_FILE:stpp> ${args} ${CMAKE_CURRENT_SOURCE_DIR}/device.artpp ${out_file}
        MAIN_DEPENDENCY device.artpp
        COMMENT "Generating variant file: ${out_file}" 
        VERBATIM)

    # Compile with artic
    set(_objs ) # Reset list
    set(_interface_name driver_interface_${var_name})
    anydsl_runtime_wrap(_objs
        NAME "driver_${var_name}"
        FRONTEND "artic"
        CLANG_FLAGS ${IG_ARTIC_CLANG_FLAGS}
        ARTIC_FLAGS ${IG_ARTIC_FLAGS} --log-level info -Wall
        FILES ${ARTIC_MAIN_SRC} ${out_file} tonemapper.art tracer.art)

    # Construct shared library for runtime loading
    set(_target_name ig_driver_${var_name})
    add_library(${_target_name} MODULE ${_objs} glue.cpp ShallowArray.h)
    set_target_properties(${_target_name} PROPERTIES PREFIX "")
    target_compile_definitions(${_target_name} PRIVATE ${args})
    target_link_libraries(${_target_name} PRIVATE ${AnyDSL_runtime_LIBRARIES} ${AnyDSL_runtime_ARTIC_JIT_LIBRARIES} ig_lib_runtime TBB::tbb)
    target_include_directories(${_target_name} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
    target_include_directories(${_target_name} PRIVATE ${AnyDSL_runtime_INCLUDE_DIRS})
    add_dependencies(${_target_name} artic_c_interface)
    add_lto(${_target_name})
    add_checks(${_target_name})
    install(TARGETS ${_target_name} COMPONENT runtime LIBRARY DESTINATION lib/)

    # Some of the compilation process requires a lot of memory.
    # Prevent parallel build of the drivers (per default) by chaining them together
    # This will rebuild all the drivers up the line,
    # however drivers are dynamically defined and ment to be rebuild anyway.
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
