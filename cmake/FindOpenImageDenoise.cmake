# Locate Intel OpenImageDenoise
# This module defines
#  OpenImageDenoise_FOUND, if false, do not try to link to OpenImageDenoise
#  OpenImageDenoise_LIBRARIES
#  OpenImageDenoise_INCLUDE_DIR
#
# Setups targets
#  OpenImageDenoise::OpenImageDenoise
#
# Calling convection: <OpenImageDenoise/oidn.h>

if(OpenImageDenoise_FOUND)
  return()
endif()

set(OpenImageDenoise_FOUND FALSE)
set(OpenImageDenoise_INCLUDE_DIRS)
set(OpenImageDenoise_LIBRARIES)

set(_def_search_paths
  ~/Library/Frameworks
  /Library/Frameworks
  /sw # Fink
  /opt/csw # Blastwave
  /opt
  /usr
  "${CMAKE_LIBRARY_PATH}"
  "${OpenImageDenoise_DIR}"
)

find_path(OpenImageDenoise_INCLUDE_DIR 
  NAMES 
    OpenImageDenoise/oidn.h
  HINTS
    ENV OpenImageDenoise_HOME
  PATH_SUFFIXES include/ local/include/
  PATHS ${_def_search_paths}
)

set(OpenImageDenoise_VERSION_FILE "${OpenImageDenoise_INCLUDE_DIR}/OpenImageDenoise/config.h")

get_filename_component(_OpenImageDenoise_ROOT_DIR ${OpenImageDenoise_INCLUDE_DIR} DIRECTORY)

if(OpenImageDenoise_INCLUDE_DIR)
  set(OpenImageDenoise_INCLUDE_DIRS "${OpenImageDenoise_INCLUDE_DIR}")
endif()

# Extract the version
if(EXISTS OpenImageDenoise_VERSION_FILE)
  file(READ "${OpenImageDenoise_VERSION_FILE}" _oidn_version_file)
  string(REGEX REPLACE ".*#define[ \t\r\n]+OIDN_VERSION_MAJOR[ \t\r\n]+([0-9]+).*" "\\1"
      OpenImageDenoise_VERSION_MAJOR "${_oidn_version_file}")
  string(REGEX REPLACE ".*#define[ \t\r\n]+OIDN_VERSION_MINOR[ \t\r\n]+([0-9]+).*" "\\1"
      OpenImageDenoise_VERSION_MINOR "${_oidn_version_file}")
  string(REGEX REPLACE ".*#define[ \t\r\n]+OIDN_VERSION_PATCH[ \t\r\n]+([0-9]+).*" "\\1"
      OpenImageDenoise_VERSION_PATCH "${_oidn_version_file}")
  
  set(OpenImageDenoise_VERSION "${OpenImageDenoise_VERSION_MAJOR}.${OpenImageDenoise_VERSION_MINOR}.${OpenImageDenoise_VERSION_PATCH}")
endif()

function(_OpenImageDenoise_FIND_LIB setname)
  find_library(${setname}
    NAMES ${ARGN}
    HINTS
      ENV OpenImageDenoise_HOME
    PATH_SUFFIXES lib
    PATHS ${_def_search_paths} "${_OpenImageDenoise_ROOT_DIR}"
  )
endfunction()

# Allowed components
set(_OpenImageDenoise_COMPONENTS
OpenImageDenoise
)

set(_OpenImageDenoise_OpenImageDenoise_DEPS )
set(_OpenImageDenoise_OpenImageDenoise_LIBNAMES OpenImageDenoise )

# If COMPONENTS are not given, set it to default
if(NOT OpenImageDenoise_FIND_COMPONENTS)
  set(OpenImageDenoise_FIND_COMPONENTS ${_OpenImageDenoise_COMPONENTS})
endif()

foreach(component ${OpenImageDenoise_FIND_COMPONENTS})
  if(NOT ${component} IN_LIST _OpenImageDenoise_COMPONENTS)
    message(ERROR "Unknown component ${component}")
  endif()

  set(_release_names )
  set(_debug_names )
  foreach(_n ${_OpenImageDenoise_${component}_LIBNAMES})
    set(_release_names ${_release_names} "${_n}")
    set(_debug_names ${_debug_names} "${_n}d" "${_n}_d")
  endforeach(_n)

  _OpenImageDenoise_FIND_LIB(OpenImageDenoise_${component}_LIBRARY_RELEASE ${_release_names})
  if(OpenImageDenoise_${component}_LIBRARY_RELEASE)
    _OpenImageDenoise_FIND_LIB(OpenImageDenoise_${component}_LIBRARY_DEBUG ${_debug_names} ${_release_names})
    if(NOT OpenImageDenoise_${component}_LIBRARY_DEBUG)
      set(OpenImageDenoise_${component}_LIBRARY_DEBUG "${OpenImageDenoise_${component}_LIBRARY_RELEASE}")
    endif()

    if(NOT "${OpenImageDenoise_${component}_LIBRARY_RELEASE}" STREQUAL "${OpenImageDenoise_${component}_LIBRARY_DEBUG}")
      set(OpenImageDenoise_${component}_LIBRARY
        optimized "${OpenImageDenoise_${component}_LIBRARY_RELEASE}" debug "${OpenImageDenoise_${component}_LIBRARY_DEBUG}")
    else()
      set(OpenImageDenoise_${component}_LIBRARY "${OpenImageDenoise_${component}_LIBRARY_RELEASE}")
    endif()

    if(OpenImageDenoise_${component}_LIBRARY)
      set(OpenImageDenoise_${component}_FOUND TRUE)
    endif()

    set(OpenImageDenoise_LIBRARIES ${OpenImageDenoise_LIBRARIES} ${OpenImageDenoise_${component}_LIBRARY})
  endif()

  mark_as_advanced(OpenImageDenoise_${component}_LIBRARY_RELEASE OpenImageDenoise_${component}_LIBRARY_DEBUG)
endforeach(component)

# Setup targets
foreach(component ${OpenImageDenoise_FIND_COMPONENTS})
  if(OpenImageDenoise_${component}_FOUND)
    set(_deps )

    foreach(dependency ${_OpenImageDenoise_${component}_DEPS})
      set(_deps ${_deps} OpenImageDenoise::${component})
    endforeach(dependency)

    add_library(OpenImageDenoise::${component} SHARED IMPORTED)
    set_target_properties(OpenImageDenoise::${component} PROPERTIES
      IMPORTED_LOCATION_RELEASE         "${OpenImageDenoise_${component}_LIBRARY_RELEASE}"
      IMPORTED_LOCATION_MINSIZEREL      "${OpenImageDenoise_${component}_LIBRARY_RELEASE}"
      IMPORTED_LOCATION_RELWITHDEBINFO  "${OpenImageDenoise_${component}_LIBRARY_RELEASE}"
      IMPORTED_LOCATION_DEBUG           "${OpenImageDenoise_${component}_LIBRARY_DEBUG}"
      ## TODO: Set this to a proper value (on windows -> dll)
      IMPORTED_IMPLIB_RELEASE           "${OpenImageDenoise_${component}_LIBRARY_RELEASE}"
      IMPORTED_IMPLIB_MINSIZEREL        "${OpenImageDenoise_${component}_LIBRARY_RELEASE}"
      IMPORTED_IMPLIB_RELWITHDEBINFO    "${OpenImageDenoise_${component}_LIBRARY_RELEASE}"
      IMPORTED_IMPLIB_DEBUG             "${OpenImageDenoise_${component}_LIBRARY_DEBUG}"
      INTERFACE_LINK_LIBRARIES          "${_deps}"
      INTERFACE_INCLUDE_DIRECTORIES     "${OpenImageDenoise_INCLUDE_DIRS}"
    )
  endif()
endforeach(component)

if(OpenImageDenoise_INCLUDE_DIRS AND OpenImageDenoise_LIBRARIES)
	set(OpenImageDenoise_FOUND TRUE)
endif()

if(OpenImageDenoise_FOUND AND NOT OpenImageDenoise_FIND_QUIETLY)
  message(STATUS "OpenImageDenoise version: ${OpenImageDenoise_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenImageDenoise
  VERSION_VAR OpenImageDenoise_VERSION
  REQUIRED_VARS OpenImageDenoise_LIBRARIES OpenImageDenoise_INCLUDE_DIRS
  HANDLE_COMPONENTS)

mark_as_advanced(OpenImageDenoise_INCLUDE_DIR)
