set(SRC 
    jit.cpp
    jit.h
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

add_library(ig_lib_jit STATIC ${SRC} ${API_SRC_FILE})
target_link_libraries(ig_lib_jit PUBLIC std::filesystem ${AnyDSL_runtime_ARTIC_JIT_LIBRARIES})
target_include_directories(ig_lib_jit PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>)
target_compile_definitions(ig_lib_jit PUBLIC "$<$<CONFIG:Debug>:IG_DEBUG>")
target_compile_features(ig_lib_jit PUBLIC cxx_std_17)
set_target_properties(ig_lib_jit PROPERTIES POSITION_INDEPENDENT_CODE ON)
