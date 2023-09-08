option(IG_WITH_CONVERTER_MITSUBA "Build tool to convert from mitsuba to our format" OFF)

if(IG_WITH_CONVERTER_MITSUBA)
add_subdirectory(mts2ig)
endif()

add_subdirectory(exr2hdr)
add_subdirectory(exr2jpg)
add_subdirectory(exr2png)
add_subdirectory(hdr2exr)

add_subdirectory(exrinfo)

add_subdirectory(ply2obj)
add_subdirectory(obj2ply)

if (IG_HAS_UI)
add_subdirectory(profiler)
endif()
