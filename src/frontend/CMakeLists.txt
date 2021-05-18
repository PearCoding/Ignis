if(IG_HAS_UI)
    add_subdirectory(ui)
endif()

add_subdirectory(viewer)

if(IG_WITH_TRACER)
    add_subdirectory(tracer)
endif()

if(IG_HAS_PYTHON_API)
    add_subdirectory(python)
endif()