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

################################################################

set(SRC 
    IG_Config.h
    Buffer.h
    Camera.h
    Color.h 
    Image.cpp
    Image.h
    Loader.cpp
    Loader.h
    Logger.cpp
    Logger.h
    Parser.cpp
    Parser.h
    Runtime.cpp
    Runtime.h
    SharedLibrary.cpp
    SharedLibrary.h
    Target.h
    bvh/BVH.h
    bvh/MemoryPool.h
    driver/Configuration.h
    driver/DriverManager.cpp
    driver/DriverManager.h
    driver/Interface.h
    klems/KlemsLoader.cpp
    klems/KlemsLoader.h
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
set_source_files_properties(${HW_HEADER} PROPERTIES GENERATED ON)
add_library(ig_lib_runtime STATIC ${SRC} ${HW_HEADER})
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(ig_lib_runtime PUBLIC Threads::Threads)
endif()
target_link_libraries(ig_lib_runtime PUBLIC Eigen3::Eigen PRIVATE ${CMAKE_DL_LIBS} ${LZ4_LIBRARY} OpenImageIO::OpenImageIO pugixml)
target_include_directories(ig_lib_runtime PRIVATE ${tinyobjloader_SOURCE_DIR} ${rapidjson_SOURCE_DIR}/include)
target_include_directories(ig_lib_runtime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_features(ig_lib_runtime PUBLIC cxx_std_17)
set_target_properties(ig_lib_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)

