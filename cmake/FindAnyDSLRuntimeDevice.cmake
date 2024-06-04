# Special purpose cmake library to load AnyDSL runtimes for specific devices

# Allowed components
set(_AnyDSLRuntimeDevice_COMPONENTS cpu cuda) # TODO

if(NOT AnyDSLRuntimeDevice_FIND_COMPONENTS)
  set(AnyDSLRuntimeDevice_FIND_COMPONENTS ${_AnyDSLRuntimeDevice_COMPONENTS})
endif()

set(_def_search_paths
  ~/Library/Frameworks
  /Library/Frameworks
  /sw # Fink
  /opt/csw # Blastwave
  /opt
  /usr
  "${CMAKE_LIBRARY_PATH}"
  "${ANYDSL_DIR}"
)

function(_AnyDSLRuntimeDevice_FIND_LIB setname)
    find_library(${setname}
        NAMES ${ARGN}
        HINTS
        ENV ANYDSL_HOME
        PATH_SUFFIXES lib
        PATHS ${_def_search_paths}
    )
endfunction()

set(AnyDSLRuntimeDevice_FOUND FALSE)
set(AnyDSLRuntimeDevice_INCLUDE_DIRS )
set(AnyDSLRuntimeDevice_LIBRARIES )

foreach(component ${AnyDSLRuntimeDevice_FIND_COMPONENTS})
    if(NOT ${component} IN_LIST _AnyDSLRuntimeDevice_COMPONENTS)
        message(ERROR "Unknown component ${component}")
    endif()

    find_path(AnyDSLRuntimeDevice_${component}_INCLUDE_DIR anydsl_runtime.h
        HINTS
            ENV ANYDSL_HOME
        PATH_SUFFIXES include/ local/include/
        PATHS ${_def_search_paths}
    )

    get_filename_component(_AnyDSLRuntimeDevice_${component}_ROOT_DIR "${AnyDSLRuntimeDevice_${component}_INCLUDE_DIR}" DIRECTORY)

    find_path(AnyDSLRuntimeDevice_${component}_INCLUDE_CONFIG_DIR anydsl_runtime_config.h
        PATH_SUFFIXES include/ local/include/
        PATHS ${_def_search_paths} ${_AnyDSLRuntimeDevice_${component}_ROOT_DIR}/build
    )

    set(AnyDSLRuntimeDevice_${component}_INCLUDE_DIRS "${AnyDSLRuntimeDevice_${component}_INCLUDE_DIR}" "${AnyDSLRuntimeDevice_${component}_INCLUDE_CONFIG_DIR}")
    set(AnyDSLRuntimeDevice_INCLUDE_DIRS ${AnyDSLRuntimeDevice_INCLUDE_DIRS} ${AnyDSLRuntimeDevice_${component}_INCLUDE_DIRS})

    _AnyDSLRuntimeDevice_FIND_LIB(AnyDSLRuntimeDevice_${component}_LIBRARY_RELEASE runtime)
    _AnyDSLRuntimeDevice_FIND_LIB(AnyDSLRuntimeDevice_${component}_LIBRARY_JIT_RELEASE runtime)
    if(AnyDSLRuntimeDevice_${component}_LIBRARY_RELEASE)
        _AnyDSLRuntimeDevice_FIND_LIB(AnyDSLRuntimeDevice_${component}_LIBRARY_DEBUG runtime)
        _AnyDSLRuntimeDevice_FIND_LIB(AnyDSLRuntimeDevice_${component}_LIBRARY_JIT_DEBUG runtime)
        
        if(NOT AnyDSLRuntimeDevice_${component}_LIBRARY_DEBUG)
            set(AnyDSLRuntimeDevice_${component}_LIBRARY_DEBUG "${AnyDSLRuntimeDevice_${component}_LIBRARY_RELEASE}")
        endif()
        if(NOT AnyDSLRuntimeDevice_${component}_LIBRARY_JIT_DEBUG)
            set(AnyDSLRuntimeDevice_${component}_LIBRARY_JIT_DEBUG "${AnyDSLRuntimeDevice_${component}_LIBRARY_JIT_RELEASE}")
        endif()

        if(NOT "${AnyDSLRuntimeDevice_${component}_LIBRARY_RELEASE}" STREQUAL "${AnyDSLRuntimeDevice_${component}_LIBRARY_DEBUG}")
            set(AnyDSLRuntimeDevice_${component}_LIBRARY optimized "${AnyDSLRuntimeDevice_${component}_LIBRARY_RELEASE}" debug "${AnyDSLRuntimeDevice_${component}_LIBRARY_DEBUG}")
        else()
            set(AnyDSLRuntimeDevice_${component}_LIBRARY "${AnyDSLRuntimeDevice_${component}_LIBRARY_RELEASE}")
        endif()

        if(NOT "${AnyDSLRuntimeDevice_${component}_LIBRARY_JIT_RELEASE}" STREQUAL "${AnyDSLRuntimeDevice_${component}_LIBRARY_JIT_DEBUG}")
            set(AnyDSLRuntimeDevice_${component}_LIBRARY_JIT optimized "${AnyDSLRuntimeDevice_${component}_LIBRARY_JIT_RELEASE}" debug "${AnyDSLRuntimeDevice_${component}_LIBRARY_JIT_DEBUG}")
        else()
            set(AnyDSLRuntimeDevice_${component}_LIBRARY_JIT "${AnyDSLRuntimeDevice_${component}_LIBRARY_JIT_RELEASE}")
        endif()

        if(AnyDSLRuntimeDevice_${component}_LIBRARY)
            set(AnyDSLRuntimeDevice_${component}_FOUND TRUE)
            set(AnyDSLRuntimeDevice_FOUND TRUE)
        endif()

        set(AnyDSLRuntimeDevice_LIBRARIES ${AnyDSLRuntimeDevice_LIBRARIES} ${AnyDSLRuntimeDevice_${component}_LIBRARY} ${AnyDSLRuntimeDevice_${component}_LIBRARY_JIT})
    endif()

    mark_as_advanced(
        AnyDSLRuntimeDevice_${component}_LIBRARY_RELEASE AnyDSLRuntimeDevice_${component}_LIBRARY_DEBUG 
        AnyDSLRuntimeDevice_${component}_LIBRARY_JIT_RELEASE AnyDSLRuntimeDevice_${component}_LIBRARY_JIT_DEBUG 
        AnyDSLRuntimeDevice_${component}_INCLUDE_DIR)
endforeach(component)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AnyDSLRuntimeDevice HANDLE_COMPONENTS)
