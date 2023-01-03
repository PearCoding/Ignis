# Contains main dependencies used everywhere
# Specific dependencies in optional components are defined inside the component
CPMAddPackage(
    NAME stb
    GITHUB_REPOSITORY nothings/stb 
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME tinyobjloader
    GITHUB_REPOSITORY tinyobjloader/tinyobjloader
    GIT_TAG release
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME rapidjson
    GITHUB_REPOSITORY Tencent/rapidjson
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME pugixml
    GITHUB_REPOSITORY zeux/pugixml 
    GIT_TAG master
    EXCLUDE_FROM_ALL YES
)

CPMAddPackage(
    NAME tinyexr
    GITHUB_REPOSITORY syoyo/tinyexr 
    GIT_TAG release
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME tinygltf
    GITHUB_REPOSITORY syoyo/tinygltf 
    GIT_TAG release
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME libbvh
    GITHUB_REPOSITORY madmann91/bvh
    GIT_TAG v1
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME pexpr
    GITHUB_REPOSITORY PearCoding/PExpr 
    GIT_TAG master
    EXCLUDE_FROM_ALL YES
)