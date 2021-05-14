if(IG_HAS_UI)
    add_subdirectory(ui)
endif()

add_subdirectory(viewer)
add_subdirectory(tracer)

if(IG_HAS_PYTHON_API)
    add_subdirectory(python)
endif()