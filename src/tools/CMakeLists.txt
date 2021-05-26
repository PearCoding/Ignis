option(IG_WITH_CONVERTER_MITSUBA "Build tool to convert from mitsuba to our format" ON)

if(IG_WITH_CONVERTER_MITSUBA)
    add_subdirectory(mts2ig)
endif()

add_subdirectory(exr2hdr)
add_subdirectory(hdr2exr)