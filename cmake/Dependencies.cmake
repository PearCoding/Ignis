# Single source of truth for every CPM dependency version.
#
# Each dependency is pinned to a specific tag or commit so a clean checkout always resolves
# the same versions instead of floating on a moving branch. Floating branches are how a
# stray ImPlot "master" update silently broke the viewer build. Override any pin on the
# command line, e.g. -DIG_IMPLOT_GIT_TAG=<tag-or-sha>, to build against a different version.
#
# Commit pins were taken from the versions the project was last known to build against; bump
# them deliberately and verify a build rather than tracking a branch.

# --- Core (cmake/GetDependencies.cmake) ---
set(IG_EIGEN_GIT_TAG             "5.0.1" CACHE STRING "eigen version")
set(IG_STB_GIT_TAG               "31c1ad37456438565541f4919958214b6e762fb4" CACHE STRING "stb version")
set(IG_TINYOBJLOADER_GIT_TAG     "v2.0.0rc13" CACHE STRING "tinyobjloader version")
set(IG_RAPIDJSON_GIT_TAG         "v1.1.0" CACHE STRING "rapidjson version")
set(IG_PUGIXML_GIT_TAG           "v1.16" CACHE STRING "pugixml version")
set(IG_TINYEXR_GIT_TAG           "v3.1.0" CACHE STRING "tinyexr version")
set(IG_TINYGLTF_GIT_TAG          "v3.0.0" CACHE STRING "tinygltf version")
set(IG_LIBBVH_GIT_TAG            "5a9d759c51d9028130b5f25733e2b4b2dc67718b" CACHE STRING "libbvh version")
set(IG_PEXPR_GIT_TAG             "87d31b9ab7dc8e41a92f666c819bd9d4f5d820aa" CACHE STRING "PExpr version")
set(IG_CPPTRACE_GIT_TAG          "v1.0.4" CACHE STRING "cpptrace version")

# --- UI (src/frontend/ui) ---
set(IG_GLFW_GIT_TAG              "3.4" CACHE STRING "GLFW release tag")
set(IG_IMGUI_GIT_TAG             "v1.92.8-docking" CACHE STRING "imgui version")
set(IG_IMPLOT_GIT_TAG            "d65a2bef53d32502407de3a4be80f191e2f412d7" CACHE STRING "implot version (ImPlot 1.1 API)")
set(IG_IMGUI_MARKDOWN_GIT_TAG    "64a56194772cef166ea7e2b9cdf7cb2a7f5447ca" CACHE STRING "imgui_markdown version")
set(IG_PFD_GIT_TAG               "c12ea8c9a727f5320a2b4570aee863bbede2a204" CACHE STRING "portable-file-dialogs version")

# --- Frontend / tools ---
set(IG_CLI11_GIT_TAG             "v2.6.2" CACHE STRING "CLI11 version")
set(IG_TINYPARSERMITSUBA_GIT_TAG "v0.3.3" CACHE STRING "TinyParser-Mitsuba version")

# --- Python / tests ---
set(IG_NANOBIND_GIT_TAG          "v2.13.0" CACHE STRING "nanobind version")
set(IG_CATCH2_GIT_TAG            "v3.15.1" CACHE STRING "Catch2 version")
