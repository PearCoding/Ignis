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

static void exportMeshPrimitive(const std::filesystem::path& path, const tinygltf::Model& model, const tinygltf::Primitive& primitive)
{
    if (primitive.attributes.count("POSITION") == 0) {
        IG_LOG(L_ERROR) << "glTF: Can not export mesh primitive " << path << " as it does not contain a valid POSITION attribute" << std::endl;
        return;
    }

    if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
        // TODO: Could export more
        IG_LOG(L_ERROR) << "glTF: Can not export mesh primitive " << path << " as it is not a simple list of triangles" << std::endl;
        return;
    }

    bool hasNormal  = primitive.attributes.count("NORMAL") > 0;
    bool hasTexture = primitive.attributes.count("TEXCOORD_0") > 0;

    const tinygltf::Accessor* vertices = &model.accessors[primitive.attributes.at("POSITION")];
    const tinygltf::Accessor* normals  = hasNormal ? &model.accessors[primitive.attributes.at("NORMAL")] : nullptr;
    const tinygltf::Accessor* textures = hasTexture ? &model.accessors[primitive.attributes.at("TEXCOORD_0")] : nullptr;
    const tinygltf::Accessor* indices  = &model.accessors[primitive.indices];

    if (vertices->type != TINYGLTF_TYPE_VEC3 || vertices->componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        IG_LOG(L_ERROR) << "glTF: Can not export mesh primitive " << path << " as it does contain an invalid POSITION attribute accessor" << std::endl;
        return;
    }

    if (indices->type != TINYGLTF_TYPE_SCALAR) {
        // TODO: Why not unsigned int, short or more?
        IG_LOG(L_ERROR) << "glTF: Can not export mesh primitive " << path << " as it does contain an invalid index accessor" << std::endl;
        return;
    }

    if (hasNormal) {
        if (normals->count != vertices->count || normals->type != TINYGLTF_TYPE_VEC3 || normals->componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
            IG_LOG(L_WARNING) << "glTF: Skipping normals for mesh primitive " << path << " as it does contain an invalid NORMAL attribute accessor" << std::endl;
            hasNormal = false;
        }
    }

    if (hasTexture) {
        if (textures->count != vertices->count || textures->type != TINYGLTF_TYPE_VEC2 || textures->componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
            IG_LOG(L_WARNING) << "glTF: Skipping textures for mesh primitive " << path << " as it does contain an invalid TEXCOORD_0 attribute accessor" << std::endl;
            hasTexture = false;
        }
    }

    const tinygltf::BufferView* vertexBufferView = &model.bufferViews[vertices->bufferView];
    const tinygltf::Buffer* vertexBuffer         = &model.buffers[vertexBufferView->buffer];
    const uint8* vertexData                      = (vertexBuffer->data.data() + vertexBufferView->byteOffset + vertices->byteOffset);

    const tinygltf::BufferView* normalBufferView = nullptr;
    const tinygltf::Buffer* normalBuffer         = nullptr;
    const uint8* normalData                      = nullptr;
    if (hasNormal) {
        normalBufferView = &model.bufferViews[normals->bufferView];
        normalBuffer     = &model.buffers[normalBufferView->buffer];
        normalData       = (normalBuffer->data.data() + normalBufferView->byteOffset + normals->byteOffset);
    }

    const tinygltf::BufferView* texBufferView = nullptr;
    const tinygltf::Buffer* texBuffer         = nullptr;
    const uint8* texData                      = nullptr;
    if (hasTexture) {
        texBufferView = &model.bufferViews[textures->bufferView];
        texBuffer     = &model.buffers[texBufferView->buffer];
        texData       = (texBuffer->data.data() + texBufferView->byteOffset + textures->byteOffset);
    }

    const tinygltf::BufferView* indexBufferView = &model.bufferViews[indices->bufferView];
    const tinygltf::Buffer* indexBuffer         = &model.buffers[indexBufferView->buffer];
    const uint8* indexData                      = (indexBuffer->data.data() + indexBufferView->byteOffset + indices->byteOffset);

    std::ofstream out(path.generic_u8string(), std::ios::binary);

    out << "ply\n"
        << "format binary_little_endian 1.0\n"
        << "element vertex " << vertices->count << "\n"
        << "property float x\n"
        << "property float y\n"
        << "property float z\n";
    if (hasNormal) {
        out << "property float nx\n"
            << "property float ny\n"
            << "property float nz\n";
    }
    if (hasTexture) {
        out << "property float s\n"
            << "property float t\n";
    }

    const size_t triangleCount = indices->count / 3;
    out << "element face " << triangleCount << "\n";
    out << "property list uchar int vertex_indices\n";
    out << "end_header\n";

    for (size_t i = 0; i < vertices->count; ++i) {
        int vertexByteStride = vertices->ByteStride(*vertexBufferView);
        float vx             = *(reinterpret_cast<const float*>(vertexData + vertexByteStride * i) + 0);
        float vy             = *(reinterpret_cast<const float*>(vertexData + vertexByteStride * i) + 1);
        float vz             = *(reinterpret_cast<const float*>(vertexData + vertexByteStride * i) + 2);
        out.write(reinterpret_cast<const char*>(&vx), sizeof(vx));
        out.write(reinterpret_cast<const char*>(&vy), sizeof(vy));
        out.write(reinterpret_cast<const char*>(&vz), sizeof(vz));

        if (hasNormal) {
            int normalByteStride = normals->ByteStride(*normalBufferView);
            float nx             = *(reinterpret_cast<const float*>(normalData + normalByteStride * i) + 0);
            float ny             = *(reinterpret_cast<const float*>(normalData + normalByteStride * i) + 1);
            float nz             = *(reinterpret_cast<const float*>(normalData + normalByteStride * i) + 2);
            out.write(reinterpret_cast<const char*>(&nx), sizeof(nx));
            out.write(reinterpret_cast<const char*>(&ny), sizeof(ny));
            out.write(reinterpret_cast<const char*>(&nz), sizeof(nz));
        }

        if (hasTexture) {
            int texByteStride = textures->ByteStride(*texBufferView);
            float u           = *(reinterpret_cast<const float*>(texData + texByteStride * i) + 0);
            float v           = 1 - *(reinterpret_cast<const float*>(texData + texByteStride * i) + 1);
            out.write(reinterpret_cast<const char*>(&u), sizeof(u));
            out.write(reinterpret_cast<const char*>(&v), sizeof(v));
        }
    }

    for (size_t i = 0; i < triangleCount; ++i) {
        int byteStride    = indices->ByteStride(*indexBufferView);
        const uint8* p_i0 = indexData + byteStride * (3 * i + 0);
        const uint8* p_i1 = indexData + byteStride * (3 * i + 1);
        const uint8* p_i2 = indexData + byteStride * (3 * i + 2);

        int i0;
        int i1;
        int i2;
        switch (indices->componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            i0 = *reinterpret_cast<const int8*>(p_i0);
            i1 = *reinterpret_cast<const int8*>(p_i1);
            i2 = *reinterpret_cast<const int8*>(p_i2);
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            i0 = *reinterpret_cast<const uint8*>(p_i0);
            i1 = *reinterpret_cast<const uint8*>(p_i1);
            i2 = *reinterpret_cast<const uint8*>(p_i2);
            break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
            i0 = *reinterpret_cast<const int16*>(p_i0);
            i1 = *reinterpret_cast<const int16*>(p_i1);
            i2 = *reinterpret_cast<const int16*>(p_i2);
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            i0 = *reinterpret_cast<const uint16*>(p_i0);
            i1 = *reinterpret_cast<const uint16*>(p_i1);
            i2 = *reinterpret_cast<const uint16*>(p_i2);
            break;
        default:
        case TINYGLTF_COMPONENT_TYPE_INT:
            i0 = *reinterpret_cast<const int32*>(p_i0);
            i1 = *reinterpret_cast<const int32*>(p_i1);
            i2 = *reinterpret_cast<const int32*>(p_i2);
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            i0 = (int)*reinterpret_cast<const uint32*>(p_i0);
            i1 = (int)*reinterpret_cast<const uint32*>(p_i1);
            i2 = (int)*reinterpret_cast<const uint32*>(p_i2);
            break;
        }

        uint8 count = 3;
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        out.write(reinterpret_cast<const char*>(&i0), sizeof(i0));
        out.write(reinterpret_cast<const char*>(&i1), sizeof(i1));
        out.write(reinterpret_cast<const char*>(&i2), sizeof(i2));
    }
}

static std::string getTextureName(const tinygltf::Texture& tex)
{
    std::string name = "_tex" + std::to_string(tex.source);
    if (tex.sampler >= 0)
        name += "_" + std::to_string(tex.sampler);
    return name;
}

static std::string getMaterialName(const tinygltf::Material& mat, size_t id)
{
    if (mat.name.empty())
        return "_mat" + std::to_string(id);
    else
        return mat.name;
}

static void addNode(Scene& scene, const tinygltf::Model& model, const tinygltf::Node& node, const Transformf& parent)
{
    Transformf transform = parent;
    if (node.matrix.size() == 16)
        transform *= Eigen::Map<Eigen::Matrix4d>(const_cast<double*>(node.matrix.data())).cast<float>();

    if (node.translation.size() == 3)
        transform.translate(Vector3f(node.translation[0], node.translation[1], node.translation[2]));

    if (node.scale.size() == 3)
        transform.scale(Vector3f(node.scale[0], node.scale[1], node.scale[2]));

    if (node.rotation.size() == 4)
        transform.rotate(Quaternionf(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]));

    if (node.mesh >= 0) {
        size_t primCount           = 0;
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        for (const auto& prim : mesh.primitives) {
            const tinygltf::Material& material = model.materials[prim.material];
            const std::string name             = mesh.name + "_" + std::to_string(primCount);

            auto obj = std::make_shared<Object>(OT_ENTITY, "");
            obj->setProperty("shape", Property::fromString(name));
            obj->setProperty("bsdf", Property::fromString(getMaterialName(material, (size_t)prim.material)));
            obj->setProperty("transform", Property::fromTransform(transform));
            scene.addEntity(node.name + "_" + name, obj);
            ++primCount;
        }
    }

    if (node.camera >= 0) {
        const tinygltf::Camera& camera = model.cameras[node.camera];
        if (camera.type == "orthographic") {
            auto obj = std::make_shared<Object>(OT_CAMERA, "orthographic");
            obj->setProperty("transform", Property::fromTransform(transform));
            obj->setProperty("near_clip", Property::fromNumber(camera.orthographic.znear));
            obj->setProperty("far_clip", Property::fromNumber(camera.orthographic.zfar));
            // TODO: xmag, ymag
        } else {
            auto obj = std::make_shared<Object>(OT_CAMERA, "perspective");
            obj->setProperty("transform", Property::fromTransform(transform));
            obj->setProperty("fov", Property::fromNumber(camera.perspective.yfov));
            obj->setProperty("near_clip", Property::fromNumber(camera.perspective.znear));
            obj->setProperty("far_clip", Property::fromNumber(camera.perspective.zfar));
            // TODO: aspect ratio
        }
    }

    for (int child : node.children)
        addNode(scene, model, model.nodes[child], transform);
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

    for (const auto& tex : model.textures) {
        const tinygltf::Image& img     = model.images[tex.source];
        std::filesystem::path img_path = img.uri;
        if (img_path.is_relative())
            img_path = directory / img_path;

        auto obj = std::make_shared<Object>(OT_TEXTURE, "image");
        obj->setProperty("filename", Property::fromString(std::filesystem::canonical(img_path).generic_u8string()));

        if (tex.sampler >= 0) {
            const tinygltf::Sampler& sampler = model.samplers[tex.sampler];
            switch (sampler.magFilter) {
            case TINYGLTF_TEXTURE_FILTER_NEAREST:
                obj->setProperty("filter_type", Property::fromString("nearest"));
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR:
                obj->setProperty("filter_type", Property::fromString("bilinear"));
                break;
            default:
                // Nothing
                break;
            }

            switch (sampler.wrapS) {
            default:
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
                obj->setProperty("wrap_mode_u", Property::fromString("repeat"));
                break;
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                obj->setProperty("wrap_mode_u", Property::fromString("clamp"));
                break;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                obj->setProperty("wrap_mode_u", Property::fromString("mirror"));
                break;
            }

            switch (sampler.wrapT) {
            default:
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
                obj->setProperty("wrap_mode_v", Property::fromString("repeat"));
                break;
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                obj->setProperty("wrap_mode_v", Property::fromString("clamp"));
                break;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                obj->setProperty("wrap_mode_v", Property::fromString("mirror"));
                break;
            }
        }

        scene.addTexture(getTextureName(tex), obj);
    }

    size_t matCounter = 0;
    for (const auto& mat : model.materials) {
        // TODO: Add proper support for texture views etc
        std::string name = getMaterialName(mat, matCounter);
        auto inner       = std::make_shared<Object>(OT_BSDF, "metallic_roughness");

        if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
            const tinygltf::Texture& tex = model.textures[mat.pbrMetallicRoughness.baseColorTexture.index];
            inner->setProperty("base_color", Property::fromString(getTextureName(tex)));
            inner->setProperty("base_color_scale", Property::fromVector3(Vector3f(mat.pbrMetallicRoughness.baseColorFactor[0], mat.pbrMetallicRoughness.baseColorFactor[1], mat.pbrMetallicRoughness.baseColorFactor[2])));
        } else {
            inner->setProperty("base_color", Property::fromVector3(Vector3f(mat.pbrMetallicRoughness.baseColorFactor[0], mat.pbrMetallicRoughness.baseColorFactor[1], mat.pbrMetallicRoughness.baseColorFactor[2])));
        }

        if (mat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
            const tinygltf::Texture& tex = model.textures[mat.pbrMetallicRoughness.metallicRoughnessTexture.index];
            inner->setProperty("metallic", Property::fromString(getTextureName(tex) + ".b"));
            inner->setProperty("alpha", Property::fromString(getTextureName(tex) + ".g"));
            inner->setProperty("metallic_scale", Property::fromNumber(mat.pbrMetallicRoughness.metallicFactor));
            inner->setProperty("alpha_scale", Property::fromNumber(mat.pbrMetallicRoughness.roughnessFactor));
        } else {
            inner->setProperty("metallic", Property::fromNumber(mat.pbrMetallicRoughness.metallicFactor));
            inner->setProperty("alpha", Property::fromNumber(mat.pbrMetallicRoughness.roughnessFactor));
        }

        std::shared_ptr<Object> obj = inner;
        if (mat.normalTexture.index >= 0) {
            scene.addBSDF(name + "_inner", inner);

            const tinygltf::Texture& tex = model.textures[mat.normalTexture.index];

            obj = std::make_shared<Object>(OT_BSDF, "normalmap");
            obj->setProperty("bsdf", Property::fromString(name + "_inner"));
            obj->setProperty("map", Property::fromString(getTextureName(tex)));
        }

        scene.addBSDF(name, obj);
        ++matCounter;
    }

    for (const auto& mesh : model.meshes) {
        size_t primCount = 0;
        for (const auto& prim : mesh.primitives) {
            const std::string name               = mesh.name + "_" + std::to_string(primCount);
            const std::filesystem::path ply_path = cache_dir / "meshes" / (name + ".ply");

            exportMeshPrimitive(ply_path, model, prim);
            auto obj = std::make_shared<Object>(OT_SHAPE, "ply");
            obj->setProperty("filename", Property::fromString(std::filesystem::canonical(ply_path).generic_u8string()));
            scene.addShape(name, obj);

            ++primCount;
        }
    }

    const tinygltf::Scene& gltf_scene = model.scenes[model.defaultScene];
    for (int nodeId : gltf_scene.nodes)
        addNode(scene, model, model.nodes[nodeId], Transformf::Identity());

    // TODO
    // for (const auto& light : model.lights) {
    // }

    return scene;
}
} // namespace Parser
} // namespace IG