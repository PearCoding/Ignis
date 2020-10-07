cmake_minimum_required(VERSION 3.1...3.15)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
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

MESSAGE(STATUS "Building Ignis ${Ignis_VERSION}")

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
include_directories(${OIIO_INCLUDE_DIR})

# LZ4 compression
find_package(LZ4 REQUIRED)
include_directories(${LZ4_INCLUDE_DIR})


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
