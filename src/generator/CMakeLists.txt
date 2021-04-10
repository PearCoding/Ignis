CPMFindPackage(
    NAME rapidjson
    GITHUB_REPOSITORY Tencent/rapidjson
    GIT_TAG master
    DOWNLOAD_ONLY
)

set(RAPIDJSON_INCLUDE_DIR "${rapidjson_SOURCE_DIR}/include")
################################################################

set(SRC 
    main.cpp
    BVHIO.h
    Generator.cpp
    Generator.h
    GeneratorBSDF.cpp
    GeneratorBSDF.h
    GeneratorContext.cpp
    GeneratorContext.h
    GeneratorEntity.cpp
    GeneratorEntity.h
    GeneratorEnvironment.cpp
    GeneratorEnvironment.h
    GeneratorLight.cpp
    GeneratorLight.h
    GeneratorShape.cpp
    GeneratorShape.h
    GeneratorTexture.cpp
    GeneratorTexture.h
    IO.cpp
    IO.h
    Loader.cpp
    Loader.h
    SceneBVHAdapter.h
    Target.h
    TriBVHAdapter.h
)

add_executable(iggenerator ${SRC})
add_dependencies(iggenerator artic_c_interface)
target_link_libraries(iggenerator PUBLIC ig_lib_runtime ig_lib_skysun ZLIB::ZLIB)
target_include_directories(iggenerator PRIVATE 
    ${RAPIDJSON_INCLUDE_DIR}
    ${CMAKE_BINARY_DIR}# Required for generated_interface.h
)
