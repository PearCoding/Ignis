set(SRC
    skysun/ElevationAzimuth.h
    skysun/SkyModel.cpp
    skysun/SkyModel.h
    skysun/SkySunConfig.h
    skysun/SunLocation.cpp
    skysun/SunLocation.h
    skysun/model/ArHosekSkyModel.cpp
    skysun/model/ArHosekSkyModel.h
    skysun/model/ArHosekSkyModelData_CIEXYZ.h
    skysun/model/ArHosekSkyModelData_RGB.h
    skysun/model/ArHosekSkyModelData_Spectral.h)

add_library(ig_lib_skysun STATIC ${SRC})
target_link_libraries(ig_lib_skysun PUBLIC ig_lib_runtime)
# Calling convention is skysun/
target_include_directories(ig_lib_skysun PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${INCLUDE_PREFIX}/>)
target_compile_features(ig_lib_skysun PUBLIC cxx_std_17)
set_property(TARGET ig_lib_skysun PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(ig_lib_skysun PROPERTIES EXPORT_NAME skysun)
