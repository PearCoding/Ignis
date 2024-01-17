if(NOT WIN32) 
# FIXME: There is a bug in AnyDSL preventing a successful build. No idea why
# See https://github.com/PearCoding/Ignis/issues/24
add_subdirectory(artic)
endif()
add_subdirectory(fuzzer)
add_subdirectory(integrator)
#add_subdirectory(multiple_runtimes)
#add_subdirectory(shading_system)
#add_subdirectory(units)