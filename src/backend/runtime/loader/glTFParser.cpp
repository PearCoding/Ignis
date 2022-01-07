#include "glTFParser.h"
#include "ImageIO.h"
#include "Logger.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#define TINYGLTF_USE_CPP14
#define TINYGLTF_USE_RAPIDJSON
#define TINYGLTF_NO_INCLUDE_RAPIDJSON
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

namespace IG {
namespace Parser {

struct LoadInfo {
    std::filesystem::path CacheDir;
};

static bool imageLoader(tinygltf::Image* image, const int image_idx, std::string*,
                        std::string*, int req_width, int req_height,
                        const unsigned char* bytes, int size, void* user_data)
{
    // Do not load the actual image!
    LoadInfo* info = (LoadInfo*)user_data;

    // TODO

    image->uri = (info->CacheDir / (image->name + ".exr")).generic_u8string();

    return true;
}

Scene glTFSceneParser::loadFromFile(const std::string& path, bool& ok)
{
    std::filesystem::path real_path(path);
    std::filesystem::path directory = real_path.parent_path();
    std::filesystem::path cache_dir = directory / (std::string("_ignis_cache_") + real_path.stem().generic_u8string());

    std::filesystem::create_directories(cache_dir);
    std::filesystem::create_directories(cache_dir / "images");
    std::filesystem::create_directories(cache_dir / "meshes");

    using namespace tinygltf;

    LoadInfo info;
    info.CacheDir = cache_dir / "images";

    Model model;
    TinyGLTF loader;
    std::string err;
    std::string warn;

    loader.SetImageLoader(imageLoader, &info);

    if (real_path.extension() == ".glb")
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, path);
    else
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, path);

    if (!warn.empty())
        IG_LOG(L_WARNING) << "glTF '" << path << "': " << warn << std::endl;

    if (!err.empty())
        IG_LOG(L_ERROR) << "glTF '" << path << "': " << err << std::endl;

    if (!ok)
        return {};

    Scene scene;

    for (const auto img : model.images) {
        auto obj = std::make_shared<Object>(OT_TEXTURE, "image");
        obj->setProperty("path", Property::fromString(img.uri));
        scene.addTexture(img.name, obj);
    }

    return scene;
}
} // namespace Parser
} // namespace IG