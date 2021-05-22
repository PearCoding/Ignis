# Get dependencies
CPMFindPackage(
    NAME tinyobjloader
    GITHUB_REPOSITORY tinyobjloader/tinyobjloader
    GIT_TAG master
    DOWNLOAD_ONLY
    EXCLUDE_FROM_ALL
)

CPMFindPackage(
    NAME rapidjson
    GITHUB_REPOSITORY Tencent/rapidjson
    GIT_TAG master
    DOWNLOAD_ONLY
    EXCLUDE_FROM_ALL
)

CPMAddPackage(
    NAME pugixml
    GITHUB_REPOSITORY zeux/pugixml 
    GIT_TAG master
    EXCLUDE_FROM_ALL
)

CPMAddPackage(
    NAME hwinfo
    GITHUB_REPOSITORY PearCoding/hwinfo 
    GIT_TAG main
    EXCLUDE_FROM_ALL
)

################################################################

set(SRC 
    IG_Config.h
    Camera.h
    Color.h 
    DebugMode.h
    Image.cpp
    Image.h
    Logger.cpp
    Logger.h
    Runtime.cpp
    Runtime.h
    SharedLibrary.cpp
    SharedLibrary.h
    Target.h
    bvh/BVH.h
    bvh/MemoryPool.h
    bvh/SceneBVHAdapter.h
    bvh/TriBVHAdapter.h
    config/Build.cpp
    config/Build.h
    config/Git.h.in
    config/Version.h.in
    driver/Configuration.cpp
    driver/Configuration.h
    driver/DriverManager.cpp
    driver/DriverManager.h
    driver/Interface.h
    klems/KlemsLoader.cpp
    klems/KlemsLoader.h
    loader/Loader.cpp
    loader/Loader.h
    loader/LoaderBSDF.cpp
    loader/LoaderBSDF.h
    loader/LoaderContext.cpp
    loader/LoaderContext.h
    loader/LoaderEntity.cpp
    loader/LoaderEntity.h
    loader/LoaderEnvironment.cpp
    loader/LoaderEnvironment.h
    loader/LoaderLight.cpp
    loader/LoaderLight.h
    loader/LoaderShape.cpp
    loader/LoaderShape.h
    loader/LoaderTexture.cpp
    loader/LoaderTexture.h
    loader/Parser.cpp
    loader/Parser.h
    log/ConsoleLogListener.cpp
    log/ConsoleLogListener.h
    log/FileLogListener.cpp
    log/FileLogListener.h
    log/LogListener.h
    math/BoundingBox.h
    math/Triangle.h
    mesh/MtsSerializedFile.cpp
    mesh/MtsSerializedFile.h
    mesh/ObjFile.cpp
    mesh/ObjFile.h
    mesh/PlyFile.cpp
    mesh/PlyFile.h
    mesh/Tangent.h
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
    table/DynTable.h
    table/SceneDatabase.h
)

set(HW_HEADER "${CMAKE_BINARY_DIR}/HW_Features.h")
add_custom_command(
  OUTPUT "${HW_HEADER}"
  COMMAND $<TARGET_FILE:hwinfo> --header --prefix IG > ${HW_HEADER}
  COMMENT "Generating hardware feature information: ${HW_HEADER}" 
  VERBATIM)
  
add_library(ig_lib_runtime STATIC ${SRC} ${HW_HEADER})
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(ig_lib_runtime PUBLIC Threads::Threads)
endif()
target_link_libraries(ig_lib_runtime PUBLIC Eigen3::Eigen PRIVATE ${CMAKE_DL_LIBS} OpenImageIO::OpenImageIO pugixml TBB::tbb ZLIB::ZLIB)
target_include_directories(ig_lib_runtime PRIVATE ${tinyobjloader_SOURCE_DIR} ${rapidjson_SOURCE_DIR}/include)
target_include_directories(ig_lib_runtime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>)
target_compile_definitions(ig_lib_runtime PUBLIC "$<$<CONFIG:Debug>:IG_DEBUG>")
target_compile_features(ig_lib_runtime PUBLIC cxx_std_17)
set_target_properties(ig_lib_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
