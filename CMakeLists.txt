cmake_minimum_required(VERSION 3.1...3.20)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# Prevent in source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	if(NOT DEFINED WITH_IN_SOURCE_BUILD)
		messag(FATAL_ERROR "CMake generation for Ignis is not allowed within the source directory! Define WITH_IN_SOURCE_BUILD if absolutely necessary!" )
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

# For whatever reason Ignis might be used as a subproject...
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	set(IG_SUBPROJECT OFF)
else()
	set(IG_SUBPROJECT ON)
endif()

# Set corresponding output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(CMAKE_DEBUG_POSTFIX  "_d")

set(Ignis_VENDOR "Ignis project 2020-2021")
include(Git)

message(STATUS "Building Ignis Rodent ${Ignis_VERSION}")

if(NOT IG_SUBPROJECT)
    include(CTest)
endif()

# Options
option(IG_WITH_VIEWER     "Build interactive viewer igview" ON)
option(IG_WITH_TRACER     "Build tracing frontend igtrace" ON)
option(IG_WITH_PYTHON_API "Build python API" ON)
option(IG_WITH_TOOLS      "Build tools" ON)

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

# Locate tbb
find_package(TBB REQUIRED) 

# ZLib is used in the generator
find_package(ZLIB REQUIRED)

# Use stb for image io
CPMAddPackage(
    NAME stb
    GITHUB_REPOSITORY nothings/stb 
    GIT_TAG master
    DOWNLOAD_ONLY
    EXCLUDE_FROM_ALL
)

# Enable folders for Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Make sure we use march native
option(IG_OPTIMIZE_FOR_NATIVE "Build with -march=native if possible" ON)
include(CheckCXXCompilerFlag)
if(IG_OPTIMIZE_FOR_NATIVE)
	CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
	if(COMPILER_SUPPORTS_MARCH_NATIVE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
	endif()
endif()

# Optional GUI
if(IG_WITH_VIEWER)
    find_package(SDL2)

    if(SDL2_FOUND)
        set(IG_HAS_UI ON)
	else()
    	message(WARNING "Interactive viewer was requested but no SDL2 libraries were found. Disabling viewer")
    endif()
endif()

if(IG_HAS_UI)
    message(STATUS "Building with interactive viewer")
endif()

if(IG_WITH_PYTHON_API)
	find_package (Python3 COMPONENTS Development)
	if(Python3_FOUND)
		set(IG_HAS_PYTHON_API ON)
	endif()
endif()

if(IG_HAS_PYTHON_API)
	message(STATUS "Building with Python API")
endif()

configure_file(src/backend/runtime/config/Git.h.in     ${CMAKE_CURRENT_BINARY_DIR}/config/Git.h)
configure_file(src/backend/runtime/config/Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/Version.h)

# Add warnings per default
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -pedantic)
endif()
# Traverse to underlying directories
add_subdirectory(src)
