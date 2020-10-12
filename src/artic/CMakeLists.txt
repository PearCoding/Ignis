set(ARTIC_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/color.art
    ${CMAKE_CURRENT_SOURCE_DIR}/core/common.art
    ${CMAKE_CURRENT_SOURCE_DIR}/core/cpu_common.art
    ${CMAKE_CURRENT_SOURCE_DIR}/core/matrix.art
    ${CMAKE_CURRENT_SOURCE_DIR}/core/random.art
    ${CMAKE_CURRENT_SOURCE_DIR}/core/sort.art
    ${CMAKE_CURRENT_SOURCE_DIR}/core/vector.art
    ${CMAKE_CURRENT_SOURCE_DIR}/render/camera.art
    ${CMAKE_CURRENT_SOURCE_DIR}/render/driver.art
    ${CMAKE_CURRENT_SOURCE_DIR}/render/fresnel.art
    ${CMAKE_CURRENT_SOURCE_DIR}/render/geometry.art
    ${CMAKE_CURRENT_SOURCE_DIR}/render/image.art
    ${CMAKE_CURRENT_SOURCE_DIR}/render/light.art
    ${CMAKE_CURRENT_SOURCE_DIR}/render/mapping_cpu.art
    ${CMAKE_CURRENT_SOURCE_DIR}/render/mapping_gpu.art
    ${CMAKE_CURRENT_SOURCE_DIR}/render/material.art
    ${CMAKE_CURRENT_SOURCE_DIR}/render/microfacet.art
    ${CMAKE_CURRENT_SOURCE_DIR}/render/renderer.art
    ${CMAKE_CURRENT_SOURCE_DIR}/render/scene.art
    ${CMAKE_CURRENT_SOURCE_DIR}/traversal/intersection.art
    ${CMAKE_CURRENT_SOURCE_DIR}/traversal/mapping_cpu.art
    ${CMAKE_CURRENT_SOURCE_DIR}/traversal/mapping_gpu.art
    ${CMAKE_CURRENT_SOURCE_DIR}/traversal/stack.art
)

set(ARTIC_SRC ${ARTIC_SRC} PARENT_SCOPE)

anydsl_runtime_wrap(DISCARD_TMP_OBJS
    NAME "interface"
    FILES ${ARTIC_SRC} dummy_main.impala
    INTERFACE ${CMAKE_BINARY_DIR}/generated_interface)
add_custom_target(artic_c_interface ALL DEPENDS ${CMAKE_BINARY_DIR}/generated_interface.h)