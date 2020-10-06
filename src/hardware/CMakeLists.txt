add_executable(ighwinfo hwinfo.cpp)

set(out_file "${CMAKE_BINARY_DIR}/HW_Features.h")
add_custom_command(TARGET ighwinfo
  POST_BUILD
  COMMAND $<TARGET_FILE:ighwinfo> --header --prefix IG > ${out_file}
  BYPRODUCTS "${out_file}"
  COMMENT "Generating hardware feature information: ${out_file}" 
  VERBATIM)