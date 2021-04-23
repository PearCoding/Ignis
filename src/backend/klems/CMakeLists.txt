CPMAddPackage(
    NAME pugixml
    GITHUB_REPOSITORY zeux/pugixml 
    GIT_TAG master
    EXCLUDE_FROM_ALL
)

set(SRC
    klems/KlemsLoader.cpp
    klems/KlemsLoader.h)

add_library(ig_lib_klems STATIC ${SRC})
target_link_libraries(ig_lib_klems PUBLIC ig_lib_runtime PRIVATE pugixml)
# Calling convention is klems/
target_include_directories(ig_lib_klems PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${INCLUDE_PREFIX}/>)
target_compile_features(ig_lib_klems PUBLIC cxx_std_17)
set_property(TARGET ig_lib_klems PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(ig_lib_klems PROPERTIES EXPORT_NAME klems)
