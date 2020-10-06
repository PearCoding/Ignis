include(ExternalProject)

# Get tinyobjloader
ExternalProject_Add(tinyobjloader
    GIT_REPOSITORY    git@github.com:tinyobjloader/tinyobjloader.git
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON
)
ExternalProject_Get_Property(tinyobjloader SOURCE_DIR)
set(tinyobjloader_dir "${SOURCE_DIR}")

################################################################

set(SRC 
    IG_Config.h
    Logger.cpp
    Logger.h 
    Image.cpp
    Image.h
    log/ConsoleLogListener.cpp
    log/ConsoleLogListener.h
    log/FileLogListener.cpp
    log/FileLogListener.h
    log/LogListener.h
    mesh/ObjFile.cpp
    mesh/ObjFile.h
    mesh/PlyFile.cpp
    mesh/PlyFile.h
    mesh/TriMesh.cpp
    mesh/TriMesh.h
)

add_library(ig_runtime STATIC ${SRC})
add_dependencies(ig_runtime ighwinfo tinyobjloader)
target_link_libraries(ig_runtime PUBLIC Eigen3::Eigen)
target_include_directories(ig_runtime PRIVATE ${tinyobjloader_dir})
target_include_directories(ig_runtime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_features(ig_runtime PUBLIC cxx_std_17)
