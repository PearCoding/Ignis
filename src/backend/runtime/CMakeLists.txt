# Get dependencies
CPMFindPackage(
    NAME tinyobjloader
    GITHUB_REPOSITORY tinyobjloader/tinyobjloader
    GIT_TAG master
    DOWNLOAD_ONLY
)

CPMFindPackage(
    NAME rapidjson
    GITHUB_REPOSITORY Tencent/rapidjson
    GIT_TAG master
    DOWNLOAD_ONLY
)

################################################################

set(SRC 
    IG_Config.h
    Buffer.h
    Camera.h
    Color.h
    Loader.cpp
    Loader.h
    Logger.cpp
    Logger.h 
    Image.cpp
    Image.h
    Target.h
    bvh/BVH.h
    bvh/MemoryPool.h
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
)

add_library(ig_lib_runtime STATIC ${SRC} "${CMAKE_BINARY_DIR}/HW_Features.h")
add_dependencies(ig_lib_runtime tinyobjloader ignis_drivers)
target_link_libraries(ig_lib_runtime PUBLIC Eigen3::Eigen ${LZ4_LIBRARY} OpenImageIO::OpenImageIO)
target_include_directories(ig_lib_runtime PRIVATE ${tinyobjloader_SOURCE_DIR} ${rapidjson_SOURCE_DIR}/include)
target_include_directories(ig_lib_runtime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_features(ig_lib_runtime PUBLIC cxx_std_17)
set_target_properties(ig_lib_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)

