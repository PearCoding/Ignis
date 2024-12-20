file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

###########################################################
set(IG_DISABLE_DEVICES "" CACHE STRING "Devices to not build a device for.")
mark_as_advanced(IG_DISABLE_DEVICES)

string(REPLACE " " ";" _DISABLED_DEVICES "${IG_DISABLE_DEVICES}")
list(REMOVE_DUPLICATES _DISABLED_DEVICES)
list(TRANSFORM _DISABLED_DEVICES TOUPPER)

###########################################################
find_package(CUDAToolkit)

if(CUDAToolkit_FOUND)
    if(CUDAToolkit_LIBRARY_ROOT)
        set(_CUDA_ROOT ${CUDAToolkit_LIBRARY_ROOT})
    else()
        set(_CUDA_ROOT "${CUDAToolkit_LIBRARY_DIR}/..")
    endif()

    find_file(_LIBDEVICE_LIB
        NAMES libdevice.10.bc
        HINTS ${_CUDA_ROOT}
        PATH_SUFFIXES nvvm/libdevice)

    if(_LIBDEVICE_LIB)
        message(STATUS "Redistributing ${_LIBDEVICE_LIB}")
        install(FILES ${_LIBDEVICE_LIB} DESTINATION bin COMPONENT runtime)
    else()
        message(WARNING "Could not find 'libdevice.10.bc'. Redistribution might fail")
    endif()

    set(_HAS_CUDA ON)
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
option(IG_WITH_DEVICE_CPU "Enable CPU device" ON)
option(IG_WITH_DEVICE_GPU "Enable GPU device" ON)

# cmake_dependent_option(IG_WITH_DEVICE_CPU_SIMD    "Enable CPU vectorized device"  ON "IG_WITH_DEVICE_CPU" OFF)
cmake_dependent_option(IG_WITH_DEVICE_GPU_NVIDIA "Enable NVidia GPU device" ON "IG_WITH_DEVICE_GPU;_HAS_CUDA" OFF)
cmake_dependent_option(IG_WITH_DEVICE_GPU_AMD_HSA "Enable AMD GPU device via HSA" ON "IG_WITH_DEVICE_GPU;_HAS_HSA" OFF)

# ##########################################################
set(VARIANTS)

if(IG_WITH_DEVICE_CPU)
    list(APPEND VARIANTS cpu)
endif()

if(IG_WITH_DEVICE_GPU_NVIDIA)
    list(APPEND VARIANTS cuda)
endif()

if(IG_WITH_DEVICE_GPU_AMD_HSA)
    list(APPEND VARIANTS hsa)
endif()

###########################################################
set(_targets)

foreach(var ${VARIANTS})
    string(TOUPPER "${var}" VAR_UPPER)
    set(_target_name ig_device_${var})
    message(STATUS "Adding device for ${var}")

    set(AnyDSLRuntimeDevice_LIBRARIES)
    set(AnyDSLRuntimeDevice_INCLUDE_DIRS)

    find_package(AnyDSLRuntimeDevice COMPONENTS ${var})

    if(NOT AnyDSLRuntimeDevice_${var}_FOUND)
        message(WARNING "No specific AnyDSL runtime for device ${var} found. Using default one")
        set(AnyDSLRuntimeDevice_LIBRARIES ${AnyDSL_runtime_LIBRARIES} ${AnyDSL_runtime_ARTIC_JIT_LIBRARIES})
        set(AnyDSLRuntimeDevice_INCLUDE_DIRS ${AnyDSL_runtime_INCLUDE_DIRS})
    endif()

    add_library(${_target_name} MODULE ${SRC})
    target_link_libraries(${_target_name} PUBLIC ig_runtime PRIVATE ${AnyDSLRuntimeDevice_LIBRARIES} TBB::tbb TBB::tbbmalloc)
    target_include_directories(${_target_name} SYSTEM PRIVATE ${AnyDSLRuntimeDevice_INCLUDE_DIRS})
    target_compile_definitions(${_target_name} PRIVATE $<BUILD_INTERFACE:"IG_BUILD_DEVICE_${VAR_UPPER}">)
    set_target_properties(${_target_name} PROPERTIES PREFIX "")
    ig_add_extra_options(${_target_name})
    set_target_properties(${_target_name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_target_properties(${_target_name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    install(TARGETS ${_target_name} ${_IG_RUNTIME_SET} LIBRARY DESTINATION bin COMPONENT runtime)

    # Some of the compilation process requires a lot of memory.
    # Prevent parallel build of the devices (per default) by chaining them together
    # This will rebuild all the devices up the line,
    # however devices are dynamically defined and ment to be rebuild anyway.
    if(NOT IG_BUILD_DEVICE_PARALLEL)
        list(LENGTH _targets _length)

        if(_length GREATER 0)
            list(GET _targets -1 _prev)
            add_dependencies(${_target_name} ${_prev})
        endif()
    endif()

    list(APPEND _targets ${_target_name})
endforeach()

add_custom_target(ignis_devices)
add_dependencies(ignis_devices ${_targets})
