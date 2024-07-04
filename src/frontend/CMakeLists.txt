add_subdirectory(common)

if(IG_WITH_CLI)
    add_subdirectory(cli)
endif()

if(IG_HAS_UI)
    add_subdirectory(ui)
    if(IG_WITH_VIEWER)
        add_subdirectory(view)
    endif()
    if(IG_WITH_EXPLORER)
        add_subdirectory(explorer)
    endif()
endif()

if(IG_WITH_TRACER)
    add_subdirectory(trace)
endif()

if(IG_HAS_PYTHON_API)
    add_subdirectory(python)
endif()