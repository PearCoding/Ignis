CPMFindPackage(
    NAME tinyparser_mitsuba
    GIT_REPOSITORY git@github.com:PearCoding/TinyParser-Mitsuba.git
    GIT_TAG master
)

################################################################

set(SRC 
    main.cpp
    BVHAdapter.h
    Generator.cpp
    Generator.h
    GeneratorBSDF.cpp
    GeneratorBSDF.h
    GeneratorContext.cpp
    GeneratorContext.h
    GeneratorEnvironment.cpp
    GeneratorEnvironment.h
    GeneratorLight.cpp
    GeneratorLight.h
    GeneratorShape.cpp
    GeneratorShape.h
    IO.cpp
    IO.h
    Target.h
)

add_executable(iggenerator ${SRC})
add_dependencies(iggenerator artic_c_interface)
target_link_libraries(iggenerator PUBLIC ig_lib_runtime tinyparser_mitsuba ZLIB::ZLIB)
target_include_directories(iggenerator PRIVATE ${tinymitsuba_dir})
target_include_directories(iggenerator PRIVATE ${CMAKE_BINARY_DIR}) # Required for generated_interface.h