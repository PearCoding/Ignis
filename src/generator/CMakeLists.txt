CPMFindPackage(
    NAME tinyparser_mitsuba
    GIT_REPOSITORY git@github.com:PearCoding/TinyParser-Mitsuba.git
    GIT_TAG master
)

################################################################

set(SRC 
    main.cpp
    Target.h
)

add_executable(iggenerator ${SRC})
target_link_libraries(iggenerator PUBLIC ig_lib_runtime tinyparser_mitsuba)
target_include_directories(iggenerator PRIVATE ${tinymitsuba_dir})