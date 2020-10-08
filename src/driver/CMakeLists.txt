#add_library(ig_lib_driver_base STATIC interface.cpp)
if (IG_SCENE_FILE STREQUAL "")
    message(FATAL_ERROR "Please specify a valid mitsuba scene file in the IG_SCENE_FILE variable")
endif()

set(GENERATOR_OPTIONS "")
if (NOT IG_TARGET_PLATFORM STREQUAL "")
    set(GENERATOR_OPTIONS "--target" "${IG_TARGET_PLATFORM}")
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
    CLANG_FLAGS ${ARTIC_CLANG_FLAGS}
    IMPALA_FLAGS --log-level info --max-errors 5
    FILES ${ARTIC_SRC} ${CMAKE_CURRENT_BINARY_DIR}/${GENERATED_FILE})

# Setup actual driver
SET(CMD_FILES 
    main.cpp 
    Interface.cpp
    Interface.h
    ${ARTIC_OBJS})
SET(UI_FILES 
    ${CMD_FILES}
    Pose.cpp
    Pose.h
    UI.cpp
    UI.h)

if(IG_HAS_UI)
    add_executable(ignis ${UI_FILES})
    target_compile_definitions(ignis PRIVATE "-DWITH_UI")
    target_link_libraries(ignis PRIVATE ig_lib_ui)
else()
    add_executable(ignis ${CMD_FILES})
endif()

target_link_libraries(ignis PRIVATE ${AnyDSL_runtime_LIBRARIES} ig_lib_runtime)
add_dependencies(ignis artic_c_interface)
target_include_directories(ignis PRIVATE ${CMAKE_BINARY_DIR}) # Required for generated_interface.h