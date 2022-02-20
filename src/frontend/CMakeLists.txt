add_subdirectory(common)

add_subdirectory(cli)

if(IG_HAS_UI)
    add_subdirectory(ui)
    add_subdirectory(view)
endif()

if(IG_WITH_TRACER)
    add_subdirectory(trace)
endif()

if(IG_HAS_PYTHON_API)
    add_subdirectory(python)
endif()