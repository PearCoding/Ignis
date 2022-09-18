set(SRC
  IG_Config.h
  CameraOrientation.h
  CDF.cpp
  CDF.h
  Color.h
  Image.cpp
  Image.h
  ImageIO.cpp
  ImageIO.h
  Logger.cpp
  Logger.h
  Runtime.cpp
  Runtime.h
  RuntimeInfo.cpp
  RuntimeInfo.h
  RuntimeSettings.h
  RuntimeStructs.h
  SceneObject.h
  SHA256.cpp
  SHA256.h
  Statistics.cpp
  Statistics.h
  Target.cpp
  Target.h
  Timer.h
  bvh/BvhNAdapter.h
  bvh/NArityBvh.h
  bvh/SceneBVHAdapter.h
  bvh/TriBVHAdapter.h
  config/Build.cpp
  config/Build.h
  config/Git.h.in
  config/Version.h.in
  container/PointBvh.h
  container/PointBvh.inl
  container/PointKdTree.h
  container/PointKdTree.inl
  container/PositionGetter.h
  device/Device.cpp
  device/Device.h
  device/ShallowArray.h
  light/AreaLight.cpp
  light/AreaLight.h
  light/CIELight.cpp
  light/CIELight.h
  light/DirectionalLight.cpp
  light/DirectionalLight.h
  light/EnvironmentLight.cpp
  light/EnvironmentLight.h
  light/Light.h
  light/LightHierarchy.cpp
  light/LightHierarchy.h
  light/PerezLight.cpp
  light/PerezLight.h
  light/PointLight.cpp
  light/PointLight.h
  light/SkyLight.cpp
  light/SkyLight.h
  light/SpotLight.cpp
  light/SpotLight.h
  light/SunLight.cpp
  light/SunLight.h
  loader/glTFParser.cpp
  loader/glTFParser.h
  loader/Loader.cpp
  loader/Loader.h
  loader/LoaderBSDF.cpp
  loader/LoaderBSDF.h
  loader/LoaderCamera.cpp
  loader/LoaderCamera.h
  loader/LoaderContext.cpp
  loader/LoaderContext.h
  loader/LoaderEntity.cpp
  loader/LoaderEntity.h
  loader/LoaderEnvironment.cpp
  loader/LoaderEnvironment.h
  loader/LoaderLight.cpp
  loader/LoaderLight.h
  loader/LoaderMedium.cpp
  loader/LoaderMedium.h
  loader/LoaderResult.h
  loader/LoaderShape.cpp
  loader/LoaderShape.h
  loader/LoaderTechnique.cpp
  loader/LoaderTechnique.h
  loader/LoaderTexture.cpp
  loader/LoaderTexture.h
  loader/LoaderUtils.cpp
  loader/LoaderUtils.h
  loader/Parser.cpp
  loader/Parser.h
  loader/ShadingTree.cpp
  loader/ShadingTree.h
  loader/TechniqueInfo.h
  loader/TechniqueVariant.h
  loader/Transpiler.cpp
  loader/Transpiler.h
  log/ConsoleLogListener.cpp
  log/ConsoleLogListener.h
  log/FileLogListener.cpp
  log/FileLogListener.h
  log/LogListener.h
  math/BoundingBox.h
  math/Tangent.h
  math/Triangle.h
  measured/KlemsLoader.cpp
  measured/KlemsLoader.h
  measured/TensorTreeLoader.cpp
  measured/TensorTreeLoader.h
  mesh/MtsSerializedFile.cpp
  mesh/MtsSerializedFile.h
  mesh/ObjFile.cpp
  mesh/ObjFile.h
  mesh/PlyFile.cpp
  mesh/PlyFile.h
  mesh/Triangulation.cpp
  mesh/Triangulation.h
  mesh/TriMesh.cpp
  mesh/TriMesh.h
  serialization/BufferSerializer.cpp
  serialization/BufferSerializer.h
  serialization/FileSerializer.cpp
  serialization/FileSerializer.h
  serialization/ISerializable.h
  serialization/MemorySerializer.cpp
  serialization/MemorySerializer.h
  serialization/Serializer.cpp
  serialization/Serializer.h
  serialization/Serializer.inl
  serialization/VectorSerializer.cpp
  serialization/VectorSerializer.h
  skysun/ElevationAzimuth.h
  skysun/SkyModel.cpp
  skysun/SkyModel.h
  skysun/SkySunConfig.h
  skysun/SunLocation.cpp
  skysun/SunLocation.h
  skysun/model/ArHosekSkyModel.cpp
  skysun/model/ArHosekSkyModel.h
  skysun/model/ArHosekSkyModelData_CIEXYZ.h
  skysun/model/ArHosekSkyModelData_RGB.h
  skysun/model/ArHosekSkyModelData_Spectral.h
  shader/AdvancedShadowShader.cpp
  shader/AdvancedShadowShader.h
  shader/DeviceShader.cpp
  shader/DeviceShader.h
  shader/HitShader.cpp
  shader/HitShader.h
  shader/MissShader.cpp
  shader/MissShader.h
  shader/RayGenerationShader.cpp
  shader/RayGenerationShader.h
  shader/ScriptCompiler.cpp
  shader/ScriptCompiler.h
  shader/ShaderUtils.cpp
  shader/ShaderUtils.h
  shader/TraversalShader.cpp
  shader/TraversalShader.h
  shader/UtilityShader.cpp
  shader/UtilityShader.h
  shape/PlaneShape.h
  shape/Shape.h
  shape/ShapeProvider.h
  shape/SphereProvider.cpp
  shape/SphereProvider.h
  shape/TriMeshProvider.cpp
  shape/TriMeshProvider.h
  shape/TriShape.h
  table/DynTable.h
  table/SceneDatabase.h
)

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
add_library(ig_lib_runtime SHARED ${SRC} ${API_SRC_FILE})

if(NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(ig_lib_runtime PUBLIC Threads::Threads)
endif()

target_link_libraries(ig_lib_runtime PUBLIC Eigen3::Eigen std::filesystem PRIVATE ${CMAKE_DL_LIBS} ${AnyDSL_runtime_LIBRARIES} ${AnyDSL_runtime_ARTIC_JIT_LIBRARIES} pugixml TBB::tbb TBB::tbbmalloc ZLIB::ZLIB pexpr)
target_include_directories(ig_lib_runtime PRIVATE ${AnyDSL_runtime_INCLUDE_DIRS} ${tinyobjloader_SOURCE_DIR} ${rapidjson_SOURCE_DIR}/include ${stb_SOURCE_DIR} ${tinyexr_SOURCE_DIR} ${tinygltf_SOURCE_DIR} ${libbvh_SOURCE_DIR}/include)
target_include_directories(ig_lib_runtime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>)
target_compile_definitions(ig_lib_runtime PUBLIC "$<$<CONFIG:Debug>:IG_DEBUG>")
add_dependencies(ig_lib_runtime artic_c_interface)

if(IG_WITH_ASSERTS)
  target_compile_definitions(ig_lib_runtime PUBLIC "IG_WITH_ASSERTS")
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # Get rid of annoying pragma warnings
  # Normally we would handle this via diagnostics, but https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431
  target_compile_options(ig_lib_runtime PRIVATE -Wno-unknown-pragmas)
endif()

# Extra
if(TARGET OpenImageDenoise::OpenImageDenoise)
  target_sources(ig_lib_runtime PRIVATE extra/oidn.cpp)
  target_link_libraries(ig_lib_runtime PRIVATE OpenImageDenoise::OpenImageDenoise)
  target_compile_definitions(ig_lib_runtime PRIVATE "IG_HAS_DENOISER")
endif()

target_compile_features(ig_lib_runtime PUBLIC cxx_std_17)
add_checks(ig_lib_runtime)
set_target_properties(ig_lib_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
