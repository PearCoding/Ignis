set(SRC 
    main.cpp
)

# We could drop some sources from ARTIC_SRC and let it adapt to the actual shading system we jit,
# but that is not worth the effort currently

# Grapped from AnyDSL runtime, as it was missing while jit compiling (BUG)
set(ARTIC_RUNTIME_FILES
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
target_link_libraries(ig_jit PUBLIC std::filesystem ${AnyDSL_runtime_ARTIC_JIT_LIBRARIES})
target_compile_definitions(ig_jit PUBLIC "$<$<CONFIG:Debug>:IG_DEBUG>")
target_compile_features(ig_jit PUBLIC cxx_std_17)
