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

add_executable(ig_generator ${SRC})
target_link_libraries(ig_generator PUBLIC ig_lib_runtime tinyparser_mitsuba)
target_include_directories(ig_generator PRIVATE ${tinymitsuba_dir})