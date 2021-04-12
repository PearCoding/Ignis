# This directory should be first as it is exposing ARTIC_SRC
add_subdirectory(artic)

# Some tools
add_subdirectory(hardware)
add_subdirectory(exr2hdr)
add_subdirectory(hdr2exr)
add_subdirectory(exrscale)

add_subdirectory(runtime)
add_subdirectory(skysun)
add_subdirectory(klems)
add_subdirectory(generator)
add_subdirectory(scene)
add_subdirectory(driver)
add_subdirectory(tracer)

if(IG_HAS_UI)
    add_subdirectory(ui)
endif()

if(IG_HAS_PYTHON_API)
    add_subdirectory(python)
endif()

add_subdirectory(tests)