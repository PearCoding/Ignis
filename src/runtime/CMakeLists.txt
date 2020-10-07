# Get tinyobjloader
CPMFindPackage(
    NAME tinyobjloader
    GIT_REPOSITORY git@github.com:tinyobjloader/tinyobjloader.git
    GIT_TAG master
    DOWNLOAD_ONLY
)

################################################################

set(SRC 
    IG_Config.h
    Buffer.h
    Color.h
    Logger.cpp
    Logger.h 
    Image.cpp
    Image.h
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

add_library(ig_lib_runtime STATIC ${SRC})
add_dependencies(ig_lib_runtime ighwinfo tinyobjloader)
target_link_libraries(ig_lib_runtime PUBLIC Eigen3::Eigen ${LZ4_LIBRARY})
target_include_directories(ig_lib_runtime PRIVATE ${tinyobjloader_SOURCE_DIR})
target_include_directories(ig_lib_runtime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_features(ig_lib_runtime PUBLIC cxx_std_17)
