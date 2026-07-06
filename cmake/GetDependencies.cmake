# Contains main dependencies used everywhere
# Specific dependencies in optional components are defined inside the component
CPMAddPackage(
    NAME eigen
    GITLAB_REPOSITORY libeigen/eigen
    GIT_TAG ${IG_EIGEN_GIT_TAG}
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME stb
    GITHUB_REPOSITORY nothings/stb
    GIT_TAG ${IG_STB_GIT_TAG}
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME tinyobjloader
    GITHUB_REPOSITORY tinyobjloader/tinyobjloader
    GIT_TAG ${IG_TINYOBJLOADER_GIT_TAG}
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME rapidjson
    GITHUB_REPOSITORY Tencent/rapidjson
    GIT_TAG ${IG_RAPIDJSON_GIT_TAG}
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME pugixml
    GITHUB_REPOSITORY zeux/pugixml
    GIT_TAG ${IG_PUGIXML_GIT_TAG}
    EXCLUDE_FROM_ALL YES
    SYSTEM
)

CPMAddPackage(
    NAME tinyexr
    GITHUB_REPOSITORY syoyo/tinyexr
    GIT_TAG ${IG_TINYEXR_GIT_TAG}
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME tinygltf
    GITHUB_REPOSITORY syoyo/tinygltf
    GIT_TAG ${IG_TINYGLTF_GIT_TAG}
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME libbvh
    GITHUB_REPOSITORY madmann91/bvh
    GIT_TAG ${IG_LIBBVH_GIT_TAG}
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME pexpr
    GITHUB_REPOSITORY PearCoding/PExpr
    GIT_TAG ${IG_PEXPR_GIT_TAG}
    EXCLUDE_FROM_ALL YES
)

CPMAddPackage(
    NAME cpptrace
    GITHUB_REPOSITORY jeremy-rifkin/cpptrace
    GIT_TAG ${IG_CPPTRACE_GIT_TAG}
    EXCLUDE_FROM_ALL YES
    SYSTEM
    OPTIONS
    "CPPTRACE_BUILD_SHARED OFF"
)