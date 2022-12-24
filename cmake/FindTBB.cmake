# Locate TBB
# This module defines
#  TBB_FOUND, if false, do not try to link to TBB
#  TBB_LIBRARIES
#  TBB_INCLUDE_DIR
#
# Setups targets
#  TBB::tbb
#
# Calling convection: <tbb/version.h>

if(TBB_FOUND)
  return()
endif()

set(TBB_FOUND FALSE)
set(TBB_INCLUDE_DIRS)
set(TBB_LIBRARIES)
set(TBB_ONEAPI FALSE)

set(_def_search_paths
  ~/Library/Frameworks
  /Library/Frameworks
  /sw # Fink
  /opt/csw # Blastwave
  /opt
  /usr
  "${CMAKE_LIBRARY_PATH}"
  "${TBB_DIR}"
)

find_path(TBB_INCLUDE_DIR 
  NAMES 
    oneapi/tbb/version.h
    tbb/tbb_stddef.h
    tbb/version.h
  HINTS
    ENV TBB_HOME
  PATH_SUFFIXES include/ local/include/
  PATHS ${_def_search_paths}
)

if(EXISTS "${TBB_INCLUDE_DIR}/oneapi/tbb/version.h")
  set(TBB_ONEAPI TRUE)
  set(TBB_VERSION_FILE "${TBB_INCLUDE_DIR}/oneapi/tbb/version.h")
elseif(EXISTS "${TBB_INCLUDE_DIR}/tbb/tbb_stddef.h")
  set(TBB_VERSION_FILE "${TBB_INCLUDE_DIR}/tbb/tbb_stddef.h")
elseif(EXISTS "${TBB_INCLUDE_DIR}/tbb/version.h")
  set(TBB_VERSION_FILE "${TBB_INCLUDE_DIR}/tbb/version.h")
endif()

get_filename_component(_TBB_ROOT_DIR ${TBB_INCLUDE_DIR} DIRECTORY)

if(TBB_INCLUDE_DIR)
  if(TBB_ONEAPI)
    set(TBB_INCLUDE_DIRS "${TBB_INCLUDE_DIR}/oneapi" "${TBB_INCLUDE_DIR}")
  else()
    set(TBB_INCLUDE_DIRS "${TBB_INCLUDE_DIR}")
  endif()
endif()

# Extract the version
if(TBB_VERSION_FILE)
  file(READ "${TBB_VERSION_FILE}" _tbb_version_file)
  string(REGEX REPLACE ".*#define TBB_VERSION_MAJOR[ \t\r\n]+([0-9]+).*" "\\1"
      TBB_VERSION_MAJOR "${_tbb_version_file}")
  string(REGEX REPLACE ".*#define TBB_VERSION_MINOR[ \t\r\n]+([0-9]+).*" "\\1"
      TBB_VERSION_MINOR "${_tbb_version_file}")
  string(REGEX REPLACE ".*#define TBB_VERSION_PATCH[ \t\r\n]+([0-9]+).*" "\\1"
      TBB_VERSION_PATCH "${_tbb_version_file}")
  
  if(NOT CMAKE_MATCH_1 STREQUAL TBB_VERSION_PATCH)
    string(REGEX REPLACE ".*#define TBB_INTERFACE_VERSION[ \t\r\n]+([0-9]+).*" "\\1"
      TBB_VERSION_PATCH "${_tbb_version_file}")
  endif()

  set(TBB_VERSION "${TBB_VERSION_MAJOR}.${TBB_VERSION_MINOR}.${TBB_VERSION_PATCH}")
endif()

function(_TBB_FIND_LIB setname)
  find_library(${setname}
    NAMES ${ARGN}
    HINTS
      ENV TBB_HOME
    PATH_SUFFIXES lib
    PATHS ${_def_search_paths} "${_TBB_ROOT_DIR}"
  )
endfunction()

# Allowed components
set(_TBB_COMPONENTS
  tbb tbbmalloc
)

set(_TBB_tbb_DEPS )
set(_TBB_tbb_LIBNAMES tbb )
set(_TBB_tbbmalloc_DEPS )
set(_TBB_tbbmalloc_LIBNAMES tbbmalloc )

# If COMPONENTS are not given, set it to default
if(NOT TBB_FIND_COMPONENTS)
  set(TBB_FIND_COMPONENTS ${_TBB_COMPONENTS})
endif()

foreach(component ${TBB_FIND_COMPONENTS})
  if(NOT ${component} IN_LIST _TBB_COMPONENTS)
    message(ERROR "Unknown component ${component}")
  endif()

  set(_release_names )
  set(_debug_names )
  foreach(_n ${_TBB_${component}_LIBNAMES})
    set(_release_names ${_release_names} "${_n}")
    set(_debug_names ${_debug_names} "${_n}d" "${_n}_d")
  endforeach(_n)

  _TBB_FIND_LIB(TBB_${component}_LIBRARY_RELEASE ${_release_names})
  if(TBB_${component}_LIBRARY_RELEASE)
    _TBB_FIND_LIB(TBB_${component}_LIBRARY_DEBUG ${_debug_names} ${_release_names})
    if(NOT TBB_${component}_LIBRARY_DEBUG)
      set(TBB_${component}_LIBRARY_DEBUG "${TBB_${component}_LIBRARY_RELEASE}")
    endif()

    if(NOT "${TBB_${component}_LIBRARY_RELEASE}" STREQUAL "${TBB_${component}_LIBRARY_DEBUG}")
      set(TBB_${component}_LIBRARY
        optimized "${TBB_${component}_LIBRARY_RELEASE}" debug "${TBB_${component}_LIBRARY_DEBUG}")
    else()
      set(TBB_${component}_LIBRARY "${TBB_${component}_LIBRARY_RELEASE}")
    endif()

    if(TBB_${component}_LIBRARY)
      set(TBB_${component}_FOUND TRUE)
    endif()

    set(TBB_LIBRARIES ${TBB_LIBRARIES} ${TBB_${component}_LIBRARY})
  endif()

  mark_as_advanced(TBB_${component}_LIBRARY_RELEASE TBB_${component}_LIBRARY_DEBUG)
endforeach(component)

# Setup targets
foreach(component ${TBB_FIND_COMPONENTS})
  if(TBB_${component}_FOUND)
    set(_deps )

    foreach(dependency ${_TBB_${component}_DEPS})
      set(_deps ${_deps} TBB::${component})
    endforeach(dependency)

    add_library(TBB::${component} SHARED IMPORTED)
    set_target_properties(TBB::${component} PROPERTIES
      IMPORTED_LOCATION_RELEASE         "${TBB_${component}_LIBRARY_RELEASE}"
      IMPORTED_LOCATION_MINSIZEREL      "${TBB_${component}_LIBRARY_RELEASE}"
      IMPORTED_LOCATION_RELWITHDEBINFO  "${TBB_${component}_LIBRARY_RELEASE}"
      IMPORTED_LOCATION_DEBUG           "${TBB_${component}_LIBRARY_DEBUG}"
      ## TODO: Set this to a proper value (on windows -> dll)
      IMPORTED_IMPLIB_RELEASE           "${TBB_${component}_LIBRARY_RELEASE}"
      IMPORTED_IMPLIB_MINSIZEREL        "${TBB_${component}_LIBRARY_RELEASE}"
      IMPORTED_IMPLIB_RELWITHDEBINFO    "${TBB_${component}_LIBRARY_RELEASE}"
      IMPORTED_IMPLIB_DEBUG             "${TBB_${component}_LIBRARY_DEBUG}"
      INTERFACE_LINK_LIBRARIES          "${_deps}"
      INTERFACE_INCLUDE_DIRECTORIES     "${TBB_INCLUDE_DIRS}"
    )
  endif()
endforeach(component)

if(TBB_INCLUDE_DIRS AND TBB_LIBRARIES)
	set(TBB_FOUND TRUE)
endif()

if(TBB_FOUND AND NOT TBB_FIND_QUIETLY)
  message(STATUS "TBB version: ${TBB_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TBB
  VERSION_VAR TBB_VERSION
  REQUIRED_VARS TBB_LIBRARIES TBB_INCLUDE_DIRS
  HANDLE_COMPONENTS)

mark_as_advanced(TBB_INCLUDE_DIR)
