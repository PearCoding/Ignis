option(IG_WITH_CONVERTER_MITSUBA "Build tool to convert from mitsuba to our format" OFF)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(igutil ${SRC})
target_link_libraries(igutil PRIVATE ig_common)
target_include_directories(igutil PRIVATE ${stb_SOURCE_DIR})

if(IG_WITH_CONVERTER_MITSUBA)
    CPMAddPackage(
        NAME tinyparsermitsuba
        GITHUB_REPOSITORY PearCoding/TinyParser-Mitsuba 
        GIT_TAG master
        EXCLUDE_FROM_ALL YES
    )

    target_compile_definitions(igutil PRIVATE "IG_WITH_CONVERTER_MITSUBA")
    target_link_libraries(igutil PRIVATE tinyparser_mitsuba)
endif()

ig_add_extra_options(igutil)
install(TARGETS igutil ${_IG_RUNTIME_SET} COMPONENT tools)