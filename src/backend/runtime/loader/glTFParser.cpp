#include "glTFParser.h"
#include "ImageIO.h"
#include "Logger.h"

#include <unordered_set>

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
static bool imageLoader(tinygltf::Image* img, const int, std::string*,
                        std::string*, int, int,
                        const unsigned char* ptr, int size, void*)
{
    // Do nothing, except copy if necessary
    // This will only be called for embedded data uris
    img->image.resize(size);
    std::memcpy(img->image.data(), ptr, size);

    return true;
}

static std::filesystem::path exportImage(const tinygltf::Image& img, const tinygltf::Model& model, int id, const std::filesystem::path& dir)
{
    if (img.bufferView >= 0) {
        const tinygltf::BufferView& view = model.bufferViews[img.bufferView];
        const tinygltf::Buffer& buffer   = model.buffers[view.buffer];

        std::string extension = "." + tinygltf::MimeToExt(img.mimeType);

        std::filesystem::path path = dir / ("_img_" + std::to_string(id) + extension);
        std::ofstream out(path.generic_u8string(), std::ios::binary);
        out.write(reinterpret_cast<const char*>(buffer.data.data() + view.byteOffset), view.byteLength);

        return path;
    } else if (!img.image.empty()) {
        std::string extension = "." + tinygltf::MimeToExt(img.mimeType);

        std::filesystem::path path = dir / ("_img_" + std::to_string(id) + extension);
        std::ofstream out(path.generic_u8string(), std::ios::binary);
        out.write(reinterpret_cast<const char*>(img.image.data()), img.image.size());

        return path;
    } else {
        return img.uri;
    }
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

inline static bool isMaterialEmissive(const tinygltf::Material& mat)
{
    return mat.emissiveTexture.index >= 0
           || (mat.emissiveFactor.size() == 3 && (mat.emissiveFactor[0] > 0 || mat.emissiveFactor[1] > 0 || mat.emissiveFactor[2] > 0));
}

static void addNode(Scene& scene, const std::filesystem::path& baseDir, const tinygltf::Model& model, const tinygltf::Node& node, const Transformf& parent)
{
    Transformf transform = parent;
    if (node.matrix.size() == 16)
        transform *= Eigen::Map<Eigen::Matrix4d>(const_cast<double*>(node.matrix.data())).cast<float>();

    if (node.translation.size() == 3)
        transform.translate(Vector3f(node.translation[0], node.translation[1], node.translation[2]));

    if (node.scale.size() == 3)
        transform.scale(Vector3f(node.scale[0], node.scale[1], node.scale[2]));

    if (node.rotation.size() == 4)
        transform.rotate(Quaternionf(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]));

    if (node.mesh >= 0) {
        size_t primCount           = 0;
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        for (const auto& prim : mesh.primitives) {
            // TODO: Support default material
            const tinygltf::Material& material = model.materials[prim.material];
            const std::string name             = mesh.name + "_" + std::to_string(node.mesh) + "_" + std::to_string(primCount);

            auto obj = std::make_shared<Object>(OT_ENTITY, "", baseDir);
            obj->setProperty("shape", Property::fromString(name));
            obj->setProperty("bsdf", Property::fromString(getMaterialName(material, (size_t)prim.material)));
            obj->setProperty("transform", Property::fromTransform(transform));

            const std::string entity_name = node.name + std::to_string(scene.entities().size()) + "_" + name;
            scene.addEntity(entity_name, obj);

            if (isMaterialEmissive(material)) {
                auto light = std::make_shared<Object>(OT_LIGHT, "area", baseDir);
                light->setProperty("entity", Property::fromString(entity_name));

                float strength = 1;
                if (material.extensions.count("KHR_materials_emissive_strength")) {
                    const auto& ext = material.extensions.at("KHR_materials_emissive_strength");
                    if (ext.Has("emissiveStrength") && ext.Get("emissiveStrength").IsNumber()) {
                        strength = ext.Get("emissiveStrength").GetNumberAsDouble();
                    }
                }

                if (material.emissiveTexture.index >= 0) {
                    const tinygltf::Texture& tex = model.textures[material.emissiveTexture.index];
                    light->setProperty("radiance", Property::fromString(getTextureName(tex)));
                    light->setProperty("radiance_scale", Property::fromVector3(Vector3f(material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2]) * strength));
                } else {
                    light->setProperty("radiance", Property::fromVector3(Vector3f(material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2]) * strength));
                }

                scene.addLight("_light_" + entity_name, light);
            }

            ++primCount;
        }
    }

    if (node.camera >= 0) {
        const tinygltf::Camera& camera = model.cameras[node.camera];
        if (camera.type == "orthographic") {
            auto obj = std::make_shared<Object>(OT_CAMERA, "orthographic", baseDir);
            obj->setProperty("transform", Property::fromTransform(transform));
            obj->setProperty("near_clip", Property::fromNumber(camera.orthographic.znear));
            obj->setProperty("far_clip", Property::fromNumber(camera.orthographic.zfar));
            // TODO: xmag, ymag
        } else {
            auto obj = std::make_shared<Object>(OT_CAMERA, "perspective", baseDir);
            obj->setProperty("transform", Property::fromTransform(transform));
            obj->setProperty("fov", Property::fromNumber(camera.perspective.yfov));
            obj->setProperty("near_clip", Property::fromNumber(camera.perspective.znear));
            obj->setProperty("far_clip", Property::fromNumber(camera.perspective.zfar));
            // TODO: aspect ratio
        }
    }

    if (node.extensions.count("KHR_lights_punctual") > 0) {
        const auto& ext = node.extensions.at("KHR_lights_punctual");
        if (ext.Has("light") && ext.Get("light").IsInt()) {
            int lightID = ext.Get("light").GetNumberAsInt();

            if (lightID >= 0 && lightID < (int)model.lights.size()) {
                const auto& light = model.lights[lightID];

                Vector3f color = Vector3f::Ones() * light.intensity;
                if (light.color.size() == 3)
                    color = Vector3f(light.color[0], light.color[1], light.color[2]) * light.intensity;

                std::string type;
                if (light.type == "point" || light.type == "spot") {
                    // No support for spot lights
                    auto obj = std::make_shared<Object>(OT_LIGHT, "point", baseDir);
                    obj->setProperty("position", Property::fromVector3(transform * Vector3f::Zero()));
                    obj->setProperty("intensity", Property::fromVector3(color));
                    scene.addLight("_l_" + std::to_string(scene.lights().size()), obj);
                } else {
                    Vector3f dir = (transform.linear().inverse().transpose() * Vector3f(0.0f, 0.0f, -1.0f)).normalized();
                    auto obj     = std::make_shared<Object>(OT_LIGHT, "directional", baseDir);
                    obj->setProperty("direction", Property::fromVector3(dir));
                    obj->setProperty("irradiance", Property::fromVector3(color));
                    scene.addLight("_l_" + std::to_string(scene.lights().size()), obj);
                }
            }
        }
    }

    for (int child : node.children)
        addNode(scene, baseDir, model, model.nodes[child], transform);
}

inline int getTextureIndex(const tinygltf::Value& val, const std::string& name)
{
    if (val.Has(name) && val.Get(name).IsObject()) {
        const auto& info = val.Get(name);
        if (info.Has("index") && info.Get("index").IsInt())
            return info.Get("index").GetNumberAsDouble();
    }
    return -1;
}

Scene glTFSceneParser::loadFromFile(const std::filesystem::path& path, bool& ok)
{
    std::filesystem::path directory = path.parent_path();
    std::filesystem::path cache_dir = directory / (std::string("_ignis_cache_") + path.stem().generic_u8string());

    std::filesystem::create_directories(cache_dir);
    std::filesystem::create_directories(cache_dir / "images");
    std::filesystem::create_directories(cache_dir / "meshes");

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    loader.SetImageLoader(imageLoader, nullptr);

    if (path.extension() == ".glb")
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, path.generic_u8string());
    else
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, path.generic_u8string());

    if (!warn.empty())
        IG_LOG(L_WARNING) << "glTF '" << path << "': " << warn << std::endl;

    if (!err.empty())
        IG_LOG(L_ERROR) << "glTF '" << path << "': " << err << std::endl;

    if (!ok)
        return {};

    Scene scene;

    std::unordered_map<int, std::filesystem::path> loaded_images;
    for (const auto& tex : model.textures) {
        std::filesystem::path img_path;
        if (loaded_images.count(tex.source) > 0) {
            img_path = loaded_images[tex.source];
        } else {
            const tinygltf::Image& img = model.images[tex.source];
            img_path                   = exportImage(img, model, tex.source, cache_dir / "images");

            if (img_path.is_relative())
                img_path = directory / img_path;

            loaded_images[tex.source] = img_path;
        }

        auto obj = std::make_shared<Object>(OT_TEXTURE, "image", directory);
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
        auto bsdf        = std::make_shared<Object>(OT_BSDF, "principled", directory);

        if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
            const tinygltf::Texture& tex = model.textures[mat.pbrMetallicRoughness.baseColorTexture.index];
            bsdf->setProperty("base_color", Property::fromString(getTextureName(tex)));
            bsdf->setProperty("base_color_scale", Property::fromVector3(Vector3f(mat.pbrMetallicRoughness.baseColorFactor[0], mat.pbrMetallicRoughness.baseColorFactor[1], mat.pbrMetallicRoughness.baseColorFactor[2])));
        } else {
            bsdf->setProperty("base_color", Property::fromVector3(Vector3f(mat.pbrMetallicRoughness.baseColorFactor[0], mat.pbrMetallicRoughness.baseColorFactor[1], mat.pbrMetallicRoughness.baseColorFactor[2])));
        }

        if (mat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
            const tinygltf::Texture& tex = model.textures[mat.pbrMetallicRoughness.metallicRoughnessTexture.index];
            bsdf->setProperty("metallic", Property::fromString(getTextureName(tex) + ".b"));
            bsdf->setProperty("roughness", Property::fromString(getTextureName(tex) + ".g"));
            bsdf->setProperty("metallic_scale", Property::fromNumber(mat.pbrMetallicRoughness.metallicFactor));
            bsdf->setProperty("roughness_scale", Property::fromNumber(mat.pbrMetallicRoughness.roughnessFactor));
        } else {
            bsdf->setProperty("metallic", Property::fromNumber(mat.pbrMetallicRoughness.metallicFactor));
            bsdf->setProperty("roughness", Property::fromNumber(mat.pbrMetallicRoughness.roughnessFactor));
        }

        // Extensions
        if (mat.extensions.count("KHR_materials_ior") > 0) {
            const auto& ext = mat.extensions.at("KHR_materials_ior");
            if (ext.Has("ior") && ext.Get("ior").IsNumber()) {
                bsdf->setProperty("ior", Property::fromNumber(ext.Get("ior").GetNumberAsDouble()));
            }
        }

        if (mat.extensions.count("KHR_materials_sheen") > 0) {
            const auto& ext = mat.extensions.at("KHR_materials_sheen");
            const int texID = getTextureIndex(ext, "sheenColorTexture");

            float factor = 0;
            if (ext.Has("sheenColorFactor") && ext.Get("sheenColorFactor").IsNumber())
                factor = ext.Get("sheenColorFactor").GetNumberAsInt();

            // No support for colored sheen
            if (texID >= 0) {
                const tinygltf::Texture& tex = model.textures[texID];
                bsdf->setProperty("sheen", Property::fromString(getTextureName(tex) + ".m"));
                bsdf->setProperty("sheen_scale", Property::fromNumber(factor));
            } else {
                bsdf->setProperty("sheen", Property::fromNumber(factor));
            }
        }

        // Only support for transmissionFactor & transmissionTexture
        if (mat.extensions.count("KHR_materials_transmission") > 0) {
            const auto& ext = mat.extensions.at("KHR_materials_transmission");
            const int texID = getTextureIndex(ext, "transmissionTexture");

            float factor = 0;
            if (ext.Has("transmissionFactor") && ext.Get("transmissionFactor").IsNumber())
                factor = ext.Get("transmissionFactor").GetNumberAsDouble();

            if (texID >= 0) {
                const tinygltf::Texture& tex = model.textures[texID];
                bsdf->setProperty("specular_transmission", Property::fromString(getTextureName(tex) + ".r"));
                bsdf->setProperty("specular_transmission_scale", Property::fromNumber(factor));
            } else {
                bsdf->setProperty("specular_transmission", Property::fromNumber(factor));
            }

            bool is_thin = true;
            if (mat.extensions.count("KHR_materials_volume")) {
                const auto& ext2 = mat.extensions.at("KHR_materials_transmission");

                // TODO: No support for textures
                float thickness = 0;
                if (ext2.Has("thicknessFactor") && ext2.Get("thicknessFactor").IsNumber())
                    thickness = ext2.Get("thicknessFactor").GetNumberAsDouble();
                is_thin = thickness <= FltEps;
            }

            bsdf->setProperty("thin", Property::fromBool(is_thin));
        }

        // Not ratified yet, but who cares
        if (mat.extensions.count("KHR_materials_translucency") > 0) {
            const auto& ext = mat.extensions.at("KHR_materials_translucency");
            const int texID = getTextureIndex(ext, "translucencyTexture");

            float factor = 0;
            if (ext.Has("translucencyFactor") && ext.Get("translucencyFactor").IsNumber())
                factor = ext.Get("translucencyFactor").GetNumberAsDouble();

            if (texID >= 0) {
                const tinygltf::Texture& tex = model.textures[texID];
                bsdf->setProperty("diffuse_transmission", Property::fromString(getTextureName(tex) + ".r"));
                bsdf->setProperty("diffuse_transmission_scale", Property::fromNumber(factor));
            } else {
                bsdf->setProperty("diffuse_transmission", Property::fromNumber(factor));
            }
        }

        // No support for clearcoatNormalTexture
        if (mat.extensions.count("KHR_materials_clearcoat") > 0) {
            const auto& ext = mat.extensions.at("KHR_materials_clearcoat");
            const int texID = getTextureIndex(ext, "clearcoatTexture");

            float factor = 0;
            if (ext.Has("clearcoatFactor") && ext.Get("clearcoatFactor").IsNumber())
                factor = ext.Get("clearcoatFactor").GetNumberAsDouble();

            if (texID >= 0) {
                const tinygltf::Texture& tex = model.textures[texID];
                bsdf->setProperty("clearcoat", Property::fromString(getTextureName(tex) + ".r"));
                bsdf->setProperty("clearcoat_scale", Property::fromNumber(factor));
            } else {
                bsdf->setProperty("clearcoat", Property::fromNumber(factor));
            }

            const int rTexID = getTextureIndex(ext, "clearcoatRoughnessTexture");
            float rfactor    = 0;
            if (ext.Has("clearcoatRoughnessFactor") && ext.Get("clearcoatRoughnessFactor").IsNumber())
                rfactor = ext.Get("clearcoatRoughnessFactor").GetNumberAsDouble();

            if (rTexID >= 0) {
                const tinygltf::Texture& tex = model.textures[rTexID];
                bsdf->setProperty("clearcoat_roughness", Property::fromString(getTextureName(tex) + ".g"));
                bsdf->setProperty("clearcoat_roughness_scale", Property::fromNumber(rfactor));
            } else {
                bsdf->setProperty("clearcoat_roughness", Property::fromNumber(rfactor));
            }
        }

        if (mat.normalTexture.index >= 0) {
            scene.addBSDF(name + "_normal_inner", bsdf);
            const tinygltf::Texture& tex = model.textures[mat.normalTexture.index];

            bsdf = std::make_shared<Object>(OT_BSDF, "normalmap", directory);
            bsdf->setProperty("bsdf", Property::fromString(name + "_normal_inner"));
            bsdf->setProperty("map", Property::fromString(getTextureName(tex)));
        }

        if (mat.alphaMode == "MASK") {
            scene.addBSDF(name + "_blend_inner", bsdf);

            bsdf = std::make_shared<Object>(OT_BSDF, "cutoff", directory);
            bsdf->setProperty("bsdf", Property::fromString(name + "_blend_inner"));
            bsdf->setProperty("inverted", Property::fromBool(true));

            float factor = mat.pbrMetallicRoughness.baseColorFactor[3];
            if (factor > 0 && mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                const tinygltf::Texture& tex = model.textures[mat.pbrMetallicRoughness.baseColorTexture.index];
                bsdf->setProperty("weight", Property::fromString(getTextureName(tex) + ".a"));
                bsdf->setProperty("cutoff", Property::fromNumber(mat.alphaCutoff / factor));
            } else {
                bsdf->setProperty("weight", Property::fromNumber(factor));
                bsdf->setProperty("cutoff", Property::fromNumber(mat.alphaCutoff));
            }
        } else if (mat.alphaMode == "BLEND") {
            scene.addBSDF(name + "_blend_inner", bsdf);

            bsdf = std::make_shared<Object>(OT_BSDF, "mask", directory);
            bsdf->setProperty("bsdf", Property::fromString(name + "_blend_inner"));
            bsdf->setProperty("inverted", Property::fromBool(true));

            float factor = mat.pbrMetallicRoughness.baseColorFactor[3];
            if (factor > 0 && mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                const tinygltf::Texture& tex = model.textures[mat.pbrMetallicRoughness.baseColorTexture.index];
                bsdf->setProperty("weight", Property::fromString(getTextureName(tex) + ".a"));
            } else {
                bsdf->setProperty("weight", Property::fromNumber(factor));
            }
        }

        scene.addBSDF(name, bsdf);
        ++matCounter;
    }

    size_t meshCount = 0;
    for (const auto& mesh : model.meshes) {
        size_t primCount = 0;
        for (const auto& prim : mesh.primitives) {
            const std::string name               = mesh.name + "_" + std::to_string(meshCount) + "_" + std::to_string(primCount);
            const std::filesystem::path ply_path = cache_dir / "meshes" / (name + ".ply");

            exportMeshPrimitive(ply_path, model, prim);
            auto obj = std::make_shared<Object>(OT_SHAPE, "ply", directory);
            obj->setProperty("filename", Property::fromString(std::filesystem::canonical(ply_path).generic_u8string()));
            scene.addShape(name, obj);

            ++primCount;
        }
        ++meshCount;
    }

    const tinygltf::Scene& gltf_scene = model.scenes[model.defaultScene];
    for (int nodeId : gltf_scene.nodes)
        addNode(scene, directory, model, model.nodes[nodeId], Transformf::Identity());

    return scene;
}
} // namespace Parser
} // namespace IG