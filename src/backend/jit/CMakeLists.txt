set(SRC 
    main.cpp
)

# Grapped from AnyDSL runtime
set(ARTIC_RUNTIME_FILES
    ${AnyDSL_runtime_ROOT_DIR}/platforms/artic/intrinsics.impala
    ${AnyDSL_runtime_ROOT_DIR}/platforms/artic/intrinsics_rv.impala
    ${AnyDSL_runtime_ROOT_DIR}/platforms/artic/intrinsics_cpu.impala
    ${AnyDSL_runtime_ROOT_DIR}/platforms/artic/intrinsics_hls.impala
    ${AnyDSL_runtime_ROOT_DIR}/platforms/artic/intrinsics_cuda.impala
    ${AnyDSL_runtime_ROOT_DIR}/platforms/artic/intrinsics_nvvm.impala
    ${AnyDSL_runtime_ROOT_DIR}/platforms/artic/intrinsics_amdgpu.impala
    ${AnyDSL_runtime_ROOT_DIR}/platforms/artic/intrinsics_opencl.impala
    ${AnyDSL_runtime_ROOT_DIR}/platforms/artic/intrinsics_thorin.impala
    ${AnyDSL_runtime_ROOT_DIR}/platforms/artic/runtime.impala
    ${AnyDSL_runtime_ROOT_DIR}/platforms/artic/intrinsics_math.impala
)

set(API_SRC_FILE ${CMAKE_CURRENT_BINARY_DIR}/api.cpp)
add_custom_command(
        OUTPUT "${API_SRC_FILE}"
        COMMAND $<TARGET_FILE:ig_api_collector> ${ARTIC_RUNTIME_FILES} ${ARTIC_SRC} ${API_SRC_FILE}
        DEPENDS ${ARTIC_SRC}
        COMMENT "Embedding API: ${API_SRC_FILE}" 
        VERBATIM)

add_executable(ig_jit ${SRC} ${API_SRC_FILE})
target_link_libraries(ig_jit PUBLIC std::filesystem ${AnyDSL_runtime_LIBRARIES} ${Thorin_LIBRARIES} libartic)
target_include_directories(ig_jit PUBLIC ${Thorin_INCLUDE_DIRS})
target_compile_definitions(ig_jit PUBLIC "$<$<CONFIG:Debug>:IG_DEBUG>")
target_compile_features(ig_jit PUBLIC cxx_std_17)
