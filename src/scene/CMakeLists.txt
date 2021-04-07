#add_library(ig_lib_driver_base STATIC interface.cpp)
if (IG_SCENE_FILE STREQUAL "")
    message(FATAL_ERROR "Please specify a valid scene file in the IG_SCENE_FILE variable")
endif()

if(IG_ALWAYS_BUILD_BVH)
    set(GENERATOR_OPTIONS ${GENERATOR_OPTIONS} "--force-bvh")
endif()

if (NOT IG_TARGET_PLATFORM STREQUAL "")
    set(GENERATOR_OPTIONS ${GENERATOR_OPTIONS} "--target" "${IG_TARGET_PLATFORM}")
endif()
if (NOT IG_TARGET_DEVICE STREQUAL "")
    set(GENERATOR_OPTIONS ${GENERATOR_OPTIONS} "--device" "${IG_TARGET_DEVICE}")
    if (IG_MEGAKERNEL_FUSION AND (IG_TARGET_PLATFORM STREQUAL "nvvm-megakernel" OR IG_TARGET_PLATFORM STREQUAL "amdgpu-megakernel"))
        set(GENERATOR_OPTIONS ${GENERATOR_OPTIONS} "--fusion")
    endif()
endif()

# Generate main.art (entry point)
set(GENERATED_FILE main.art)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${GENERATED_FILE}
    COMMAND iggenerator ${IG_SCENE_FILE} ${GENERATOR_OPTIONS} --max-path-len ${MAX_PATH_LEN} --samples-per-pixel ${SPP} -o ${GENERATED_FILE}
    COMMAND ${CMAKE_COMMAND} -E rename ${CMAKE_BINARY_DIR}/${GENERATED_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${GENERATED_FILE}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS ${IG_SCENE_FILE} iggenerator)
add_custom_target(generate DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${GENERATED_FILE})
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${GENERATED_FILE} PROPERTIES GENERATED TRUE)

# Compile artic stuff
anydsl_runtime_wrap(ARTIC_OBJS
    NAME "artic"
    FRONTEND "artic"
    CLANG_FLAGS ${ARTIC_CLANG_FLAGS}
    ARTIC_FLAGS --log-level info --max-errors 5
    FILES ${ARTIC_SRC} ${CMAKE_CURRENT_BINARY_DIR}/${GENERATED_FILE})

add_library(ignis_scene  
    ${ARTIC_OBJS}
    Interface.cpp
    Interface.h)
target_link_libraries(ignis_scene PUBLIC ${AnyDSL_runtime_LIBRARIES} ig_lib_runtime)
add_dependencies(ignis_scene artic_c_interface)
target_include_directories(ignis_scene PUBLIC ${CMAKE_BINARY_DIR}) # Required for generated_interface.h
target_include_directories(ignis_scene PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_target_properties(ignis_scene PROPERTIES POSITION_INDEPENDENT_CODE ON)
