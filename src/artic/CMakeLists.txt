set(ARTIC_SRC
    core/color.art
    core/common.art
    core/cpu_common.art
    core/matrix.art
    core/random.art
    core/sort.art
    core/vector.art
    render/camera.art
    render/driver.art
    render/geometry.art
    render/image.art
    render/light.art
    render/mapping_cpu.art
    render/mapping_gpu.art
    render/material.art
    render/renderer.art
    render/scene.art
    traversal/intersection.art
    traversal/mapping_cpu.art
    traversal/mapping_gpu.art
    traversal/stack.art
)

anydsl_runtime_wrap(ARTIC_OBJS
    NAME "artic"
    CLANG_FLAGS ${ARTIC_CLANG_FLAGS}
    IMPALA_FLAGS --log-level info --max-errors 5
    FILES ${ARTIC_SRC} ${CMAKE_CURRENT_BINARY_DIR}/main.impala)

anydsl_runtime_wrap(DISCARD_TMP_OBJS
    NAME "interface"
    FILES ${ARTIC_SRC} dummy_main.impala
    INTERFACE ${CMAKE_BINARY_DIR}/generated_interface)
add_custom_target(artic_c_interface ALL DEPENDS ${CMAKE_BINARY_DIR}/generated_interface.h)