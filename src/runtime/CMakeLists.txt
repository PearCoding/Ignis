set(SRC
  IG_Config.h
  CacheManager.cpp
  CacheManager.h
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
  RuntimeStructs.cpp
  RuntimeStructs.h
  Scene.cpp
  Scene.h
  SceneObject.h
  SceneObjectProxy.h
  SceneProperty.h
  SHA256.cpp
  SHA256.h
  StringUtils.cpp
  StringUtils.h
  Timer.h
  bsdf/BlendBSDF.cpp
  bsdf/BlendBSDF.h
  bsdf/BSDF.cpp
  bsdf/BSDF.h
  bsdf/ConductorBSDF.cpp
  bsdf/ConductorBSDF.h
  bsdf/DielectricBSDF.cpp
  bsdf/DielectricBSDF.h
  bsdf/DiffuseBSDF.cpp
  bsdf/DiffuseBSDF.h
  bsdf/DJMeasuredBSDF.cpp
  bsdf/DJMeasuredBSDF.h
  bsdf/DoubleSidedBSDF.cpp
  bsdf/DoubleSidedBSDF.h
  bsdf/ErrorBSDF.cpp
  bsdf/ErrorBSDF.h
  bsdf/IgnoreBSDF.cpp
  bsdf/IgnoreBSDF.h
  bsdf/KlemsBSDF.cpp
  bsdf/KlemsBSDF.h
  bsdf/MapBSDF.cpp
  bsdf/MapBSDF.h
  bsdf/MaskBSDF.cpp
  bsdf/MaskBSDF.h
  bsdf/PassthroughBSDF.cpp
  bsdf/PassthroughBSDF.h
  bsdf/PhongBSDF.cpp
  bsdf/PhongBSDF.h
  bsdf/PlasticBSDF.cpp
  bsdf/PlasticBSDF.h
  bsdf/PrincipledBSDF.cpp
  bsdf/PrincipledBSDF.h
  bsdf/RadBRTDFuncBSDF.cpp
  bsdf/RadBRTDFuncBSDF.h
  bsdf/RadRoosBSDF.cpp
  bsdf/RadRoosBSDF.h
  bsdf/TensorTreeBSDF.cpp
  bsdf/TensorTreeBSDF.h
  bsdf/TransformBSDF.cpp
  bsdf/TransformBSDF.h
  bsdf/TransparentBSDF.cpp
  bsdf/TransparentBSDF.h
  bvh/BvhNAdapter.h
  bvh/NArityBvh.h
  bvh/SceneBVHAdapter.h
  bvh/TriBVHAdapter.h
  camera/Camera.cpp
  camera/Camera.h
  camera/CameraOrientation.h
  camera/FishLensCamera.cpp
  camera/FishLensCamera.h
  camera/NullCamera.cpp
  camera/NullCamera.h
  camera/OrthogonalCamera.cpp
  camera/OrthogonalCamera.h
  camera/PerspectiveCamera.cpp
  camera/PerspectiveCamera.h
  config/Build.cpp
  config/Build.h
  config/Git.h.in
  config/Version.h.in
  container/PointBvh.h
  container/PointBvh.inl
  container/PointKdTree.h
  container/PointKdTree.inl
  container/PositionGetter.h
  device/AnyDSLRuntime.cpp
  device/AnyDSLRuntime.h
  device/Device.cpp
  device/Device.h
  device/DeviceTimer.h
  device/ShaderKey.h
  device/ShallowArray.h
  device/Target.cpp
  device/Target.h
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
  loader/LoaderLight.cpp
  loader/LoaderLight.h
  loader/LoaderMedium.cpp
  loader/LoaderMedium.h
  loader/LoaderOptions.h
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
  loader/Transpiler.cpp
  loader/Transpiler.h
  log/ConsoleLogListener.cpp
  log/ConsoleLogListener.h
  log/FileLogListener.cpp
  log/FileLogListener.h
  log/LogListener.h
  math/BoundingBox.h
  math/Spherical.h
  math/Tangent.h
  math/Triangle.h
  measured/KlemsLoader.cpp
  measured/KlemsLoader.h
  measured/TensorTreeLoader.cpp
  measured/TensorTreeLoader.h
  measured/powitacq_rgb.inl
  measured/powitacq_rgb.h
  measured/djmeasured.cpp
  measured/djmeasured.h
  medium/HomogeneousMedium.cpp
  medium/HomogeneousMedium.h
  medium/Medium.cpp
  medium/Medium.h
  medium/VacuumMedium.cpp
  medium/VacuumMedium.h
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
  pattern/BrickPattern.cpp
  pattern/BrickPattern.h
  pattern/CheckerBoardPattern.cpp
  pattern/CheckerBoardPattern.h
  pattern/ExprPattern.cpp
  pattern/ExprPattern.h
  pattern/ImagePattern.cpp
  pattern/ImagePattern.h
  pattern/InvalidPattern.cpp
  pattern/InvalidPattern.h
  pattern/NoisePattern.cpp
  pattern/NoisePattern.h
  pattern/Pattern.cpp
  pattern/Pattern.h
  pattern/TransformPattern.cpp
  pattern/TransformPattern.h
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
  skysun/Illuminance.h
  skysun/PerezModel.cpp
  skysun/PerezModel.h
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
  shader/BakeShader.cpp
  shader/BakeShader.h
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
  shape/SphereShape.h
  shape/TriMeshProvider.cpp
  shape/TriMeshProvider.h
  shape/TriShape.h
  statistics/SmallShaderKey.h
  statistics/StatisticHandler.cpp
  statistics/StatisticHandler.h
  statistics/Statistics.cpp
  statistics/Statistics.h
  statistics/StatisticTypes.h
  table/DynTable.h
  table/FixTable.h
  table/SceneDatabase.h
  technique/AOTechnique.cpp
  technique/AOTechnique.h
  technique/CameraCheckTechnique.cpp
  technique/CameraCheckTechnique.h
  technique/DebugMode.cpp
  technique/DebugMode.h
  technique/DebugTechnique.cpp
  technique/DebugTechnique.h
  technique/InfoBufferTechnique.cpp
  technique/InfoBufferTechnique.h
  technique/LightTracerTechnique.cpp
  technique/LightTracerTechnique.h
  technique/LightVisibilityTechnique.cpp
  technique/LightVisibilityTechnique.h
  technique/PathTechnique.cpp
  technique/PathTechnique.h
  technique/PhotonMappingTechnique.cpp
  technique/PhotonMappingTechnique.h
  technique/Technique.cpp
  technique/Technique.h
  technique/TechniqueInfo.h
  technique/TechniqueVariant.h
  technique/VolumePathTechnique.cpp
  technique/VolumePathTechnique.h
  technique/WireframeTechnique.cpp
  technique/WireframeTechnique.h
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

target_link_libraries(ig_lib_runtime PUBLIC std::filesystem PRIVATE ${AnyDSL_runtime_LIBRARIES} pugixml TBB::tbb TBB::tbbmalloc ZLIB::ZLIB pexpr)
target_include_directories(ig_lib_runtime SYSTEM PUBLIC ${eigen_SOURCE_DIR})
target_include_directories(ig_lib_runtime SYSTEM PRIVATE ${AnyDSL_runtime_INCLUDE_DIRS} ${tinyobjloader_SOURCE_DIR} ${rapidjson_SOURCE_DIR}/include ${stb_SOURCE_DIR} ${tinyexr_SOURCE_DIR} ${tinygltf_SOURCE_DIR} ${libbvh_SOURCE_DIR}/include)
target_include_directories(ig_lib_runtime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>)
target_compile_definitions(ig_lib_runtime PUBLIC "$<$<CONFIG:Debug>:IG_DEBUG>" PRIVATE $<BUILD_INTERFACE:"IG_BUILD_LIB">)
add_dependencies(ig_lib_runtime artic_c_interface)

# There is a weird behaviour in multi-config generators and the git checker. It is fixed with the following line
get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(is_multi_config)
  add_dependencies(ig_lib_runtime check_git)
endif()

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
  
  message(STATUS "Building with OIDN support")
elseif(TARGET OpenImageDenoise)
  target_sources(ig_lib_runtime PRIVATE extra/oidn.cpp)
  target_link_libraries(ig_lib_runtime PRIVATE OpenImageDenoise)
  target_compile_definitions(ig_lib_runtime PRIVATE "IG_HAS_DENOISER")
  
  message(STATUS "Building with OIDN support")
endif()

target_compile_features(ig_lib_runtime PUBLIC cxx_std_17)
add_checks(ig_lib_runtime)
add_linker_options(ig_lib_runtime)
set_target_properties(ig_lib_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
install(TARGETS ig_lib_runtime ${_IG_RUNTIME_SET} COMPONENT runtime)
