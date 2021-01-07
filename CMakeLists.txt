cmake_minimum_required(VERSION 3.1...3.15)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# Prevent in source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	if(NOT DEFINED WITH_IN_SOURCE_BUILD)
		message(FATAL_ERROR "CMake generation for Ignis is not allowed within the source directory! Define WITH_IN_SOURCE_BUILD if absolutely necessary!" )
	endif()
endif()

# Omit superfluous "Up-to-date" messages.
if(NOT DEFINED CMAKE_INSTALL_MESSAGE)
	set(CMAKE_INSTALL_MESSAGE "LAZY")
endif()

# Set default to Release
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

project(Ignis
	VERSION 0.1
	DESCRIPTION "Experimental ray tracer")

# Set corresponding output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(CMAKE_DEBUG_POSTFIX  "_d")

MESSAGE(STATUS "Building Ignis Rodent ${Ignis_VERSION}")

# Options
set(IG_TARGET_PLATFORM "" CACHE STRING "Target platform for the generator tool. Leave empty to autodetect CPU. Use iggenerator --help to list possible platforms.")
set(IG_TARGET_DEVICE "" CACHE STRING "Target device on the selected platform. Leave empty to use the default.")
set(IG_SCENE_FILE "${PROJECT_SOURCE_DIR}/scenes/diamond_scene.json" CACHE FILEPATH "Absolute path to an Mitsuba scene")
set(IG_MEGAKERNEL_FUSION ON CACHE BOOL "Set to true to enable simple shader fusion for megakernel mappings")
set(IG_ALWAYS_BUILD_BVH OFF CACHE BOOL "Always force the BVH to be rebuilt")

# Setup artic clang flags
set(ARTIC_CLANG_FLAGS -O3 -march=native -ffast-math CACHE STRING "Artic Clang compilation options")

# Setup CPM
include(cmake/SetupCPM.cmake)

# Locate AnyDSL runtime
find_package(AnyDSL_runtime REQUIRED)
include_directories(${AnyDSL_runtime_INCLUDE_DIRS})

# Make sure the filesystem library is available
find_package(Filesystem REQUIRED)

# Locate Eigen
find_package(Eigen3 REQUIRED NO_MODULE)

# Locate OpenImageIO
find_package(OpenImageIO REQUIRED)

# LZ4 compression
find_package(LZ4 REQUIRED)
include_directories(${LZ4_INCLUDE_DIR})

# ZLib is used in the generator
find_package(ZLIB REQUIRED)

# Enable folders for Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Make sure we use march native
option(IG_OPTIMIZE_FOR_NATIVE	"Build with -march=native if possible" ON)
include(CheckCXXCompilerFlag)
if(IG_OPTIMIZE_FOR_NATIVE)
	CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
	if(COMPILER_SUPPORTS_MARCH_NATIVE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
	endif()
endif()

# Optional GUI
option(IG_ENABLE_UI	"Build with interactive viewer" ON)
if(IG_ENABLE_UI)
    find_package(SDL2)

    if(SDL2_FOUND)
        set(IG_HAS_UI ON)
    endif()
endif()

if(IG_HAS_UI)
    MESSAGE(STATUS "Building with UI")
endif()

# Traverse to underlying directories
add_subdirectory(src)
