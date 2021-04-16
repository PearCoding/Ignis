set(SRC
    serialization/BufferSerializer.cpp
    serialization/BufferSerializer.h
    serialization/FileSerializer.cpp
    serialization/FileSerializer.h
    serialization/ISerializable.h
    serialization/MemorySerializer.cpp
    serialization/MemorySerializer.h
    serialization/Serializer.cpp
    serialization/Serializer.h
    serialization/Serializer.inl)

add_library(ig_lib_serialization STATIC ${SRC})
target_link_libraries(ig_lib_serialization PUBLIC ig_lib_runtime)
# Calling convention is serialization/
target_include_directories(ig_lib_serialization PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${INCLUDE_PREFIX}/>)
target_compile_features(ig_lib_serialization PUBLIC cxx_std_17)
set_property(TARGET ig_lib_serialization PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(ig_lib_serialization PROPERTIES EXPORT_NAME serialization)
