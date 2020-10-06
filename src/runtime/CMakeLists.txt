set(SRC 
    IG_Config.h
    Logger.cpp
    Logger.h
    Image.cpp
    Image.h
    log/ConsoleLogListener.cpp
    log/ConsoleLogListener.h
    log/FileLogListener.cpp
    log/FileLogListener.h
    log/LogListener.h
    math/Common.h
)

add_library(ig_runtime STATIC ${SRC})
target_link_libraries(ig_runtime PUBLIC Eigen3::Eigen)
target_include_directories(ig_runtime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_features(ig_runtime PUBLIC cxx_std_17)
