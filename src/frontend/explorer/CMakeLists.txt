CPMAddPackage(
    NAME portable_file_dialogs
    GITHUB_REPOSITORY samhocevar/portable-file-dialogs
    GIT_TAG main
    DOWNLOAD_ONLY YES
)

set(SHADER ${CMAKE_CURRENT_SOURCE_DIR}/shader.art ${CMAKE_CURRENT_SOURCE_DIR}/glare.art)
set(SHADER_SRC_FILE ${CMAKE_CURRENT_BINARY_DIR}/shader.cpp)
add_custom_command(
  OUTPUT ${SHADER_SRC_FILE}
  COMMAND $<TARGET_FILE:ig_api_collector> --prefix ig_shader ${SHADER} ${SHADER_SRC_FILE}
  DEPENDS ${SHADER}
  VERBATIM)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(igexplorer ${SRC} ${SHADER_SRC_FILE})
target_include_directories(igexplorer SYSTEM PUBLIC ${portable_file_dialogs_SOURCE_DIR})
target_link_libraries(igexplorer PRIVATE ig_ui ig_common)
ig_add_extra_options(igexplorer)
install(TARGETS igexplorer ${_IG_RUNTIME_SET} COMPONENT frontends)
