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
    SceneBVHAdapter.h
    TriBVHAdapter.h
)

add_executable(iggenerator ${SRC})
add_dependencies(iggenerator artic_c_interface)
target_link_libraries(iggenerator PUBLIC ig_lib_runtime ig_lib_skysun ig_lib_klems ZLIB::ZLIB)
target_include_directories(iggenerator PRIVATE 
${CMAKE_BINARY_DIR}# Required for generated_interface.h
)
