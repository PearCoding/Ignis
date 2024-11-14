file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

# ##########################################################
# We could drop some sources from ARTIC_MAIN_SRC and let it adapt to the actual shading system we jit,
# but that is not worth the effort currently
set(API_SRC_FILE ${CMAKE_CURRENT_BINARY_DIR}/api.cpp)
add_custom_command(
  OUTPUT ${API_SRC_FILE}
  COMMAND $<TARGET_FILE:ig_api_collector> ${ARTIC_MAIN_SRC} ${API_SRC_FILE}
  DEPENDS ${ARTIC_MAIN_SRC}
  COMMENT "Embedding API: ${API_SRC_FILE}"
  VERBATIM)

# ##########################################################
configure_file(config/Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/Version.h)
file(GENERATE 
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/config/Flags.h 
  INPUT config/Flags.h.in)

add_library(ig_runtime SHARED ${SRC} ${API_SRC_FILE})

if(NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(ig_runtime PUBLIC Threads::Threads)
endif()

target_link_libraries(ig_runtime PUBLIC std::filesystem PRIVATE pugixml TBB::tbb TBB::tbbmalloc ZLIB::ZLIB pexpr)
target_include_directories(ig_runtime SYSTEM PUBLIC ${eigen_SOURCE_DIR})
target_include_directories(ig_runtime SYSTEM PRIVATE ${tinyobjloader_SOURCE_DIR} ${rapidjson_SOURCE_DIR}/include ${stb_SOURCE_DIR} ${tinyexr_SOURCE_DIR} ${tinygltf_SOURCE_DIR} ${libbvh_SOURCE_DIR}/src)
target_include_directories(ig_runtime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>)
target_include_directories(ig_runtime PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
target_compile_definitions(ig_runtime PUBLIC "$<$<CONFIG:Debug>:IG_DEBUG>" PRIVATE $<BUILD_INTERFACE:IG_BUILD_LIB>)
add_dependencies(ig_runtime artic_c_interface)

# There is a weird behaviour in multi-config generators and the git checker. It is fixed with the following line
get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(is_multi_config)
  add_dependencies(ig_runtime check_git)
endif()

if(IG_WITH_ASSERTS)
  target_compile_definitions(ig_runtime PUBLIC "IG_WITH_ASSERTS")
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # Get rid of annoying pragma warnings
  # Normally we would handle this via diagnostics, but https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431
  target_compile_options(ig_runtime PRIVATE -Wno-unknown-pragmas)
endif()

# Extra
if(TARGET OpenImageDenoise::OpenImageDenoise)
  target_link_libraries(ig_runtime PRIVATE OpenImageDenoise::OpenImageDenoise)
  set(HAS_OIDN ON)
elseif(TARGET OpenImageDenoise)
  target_link_libraries(ig_runtime PRIVATE OpenImageDenoise)
  set(HAS_OIDN ON)
endif()

if(HAS_OIDN)
  target_compile_definitions(ig_runtime PRIVATE "IG_HAS_DENOISER")
  if (WIN32)
    file(GLOB OIDN_DEVICES_DLLS "${CMAKE_BINARY_DIR}/bin/OpenImageDenoise_device*.dll")
    if(OIDN_DEVICES_DLLS)
      install(FILES ${OIDN_DEVICES_DLLS} TYPE BIN COMPONENT runtime)
    endif()
  endif()
  
  message(STATUS "Building with OIDN support")
endif()

target_compile_features(ig_runtime PUBLIC cxx_std_20)
ig_add_extra_options(ig_runtime)
set_target_properties(ig_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
install(TARGETS ig_runtime ${_IG_RUNTIME_SET} COMPONENT runtime)
