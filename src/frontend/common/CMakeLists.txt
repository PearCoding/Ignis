CPMAddPackage(
    NAME cli11
    GITHUB_REPOSITORY CLIUtils/CLI11
    GIT_TAG main
    DOWNLOAD_ONLY YES
)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_library(ig_common STATIC ${SRC})
target_link_libraries(ig_common PUBLIC ig_runtime TBB::tbb)
target_include_directories(ig_common SYSTEM PUBLIC ${cli11_SOURCE_DIR}/include)
target_include_directories(ig_common PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
ig_add_extra_options(ig_common)
