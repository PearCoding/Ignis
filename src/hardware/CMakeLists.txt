CPMAddPackage(
    NAME hwinfo
    GITHUB_REPOSITORY PearCoding/hwinfo 
    GIT_TAG main
    EXCLUDE_FROM_ALL
)

set(out_file "${CMAKE_BINARY_DIR}/HW_Features.h")
add_custom_target(ighwinfo
  COMMAND $<TARGET_FILE:hwinfo> --header --prefix IG > ${out_file}
  BYPRODUCTS "${out_file}"
  COMMENT "Generating hardware feature information: ${out_file}" 
  VERBATIM)