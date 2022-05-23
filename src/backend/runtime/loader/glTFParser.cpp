#include "glTFParser.h"
#include "ImageIO.h"
#include "Logger.h"

#include <string_view>
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

[[maybe_unused]] constexpr std::string_view KHR_lights_punctual             = "KHR_lights_punctual";
[[maybe_unused]] constexpr std::string_view KHR_materials_clearcoat         = "KHR_materials_clearcoat";
[[maybe_unused]] constexpr std::string_view KHR_materials_emissive_strength = "KHR_materials_emissive_strength";
[[maybe_unused]] constexpr std::string_view KHR_materials_ior               = "KHR_materials_ior";
[[maybe_unused]] constexpr std::string_view KHR_materials_sheen             = "KHR_materials_sheen";
[[maybe_unused]] constexpr std::string_view KHR_materials_translucency      = "KHR_materials_translucency";
[[maybe_unused]] constexpr std::string_view KHR_materials_transmission      = "KHR_materials_transmission";
[[maybe_unused]] constexpr std::string_view KHR_materials_unlit             = "KHR_materials_unlit";
[[maybe_unused]] constexpr std::string_view KHR_materials_volume            = "KHR_materials_volume";
[[maybe_unused]] constexpr std::string_view KHR_texture_transform           = "KHR_texture_transform";

// Uncomment this to map unlit materials to area lights. This can explode shading complexity and is therefore not really recommended
// #define IG_GLTF_MAP_UNLIT_AS_LIGHT

namespace IG::Parser {
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

inline void findAndReplaceAll(std::string& data, const std::string_view& toSearch, const std::string_view& replaceStr)
{
    size_t pos = data.find(toSearch);
    while (pos != std::string::npos) {
        data.replace(pos, toSearch.size(), replaceStr);
        pos = data.find(toSearch, pos + replaceStr.size());
    }
}

inline std::string handleURI(const std::string& uri)
{
    std::string new_uri = uri;
    findAndReplaceAll(new_uri, "%20", " ");
    return new_uri;
}

static std::filesystem::path exportImage(const tinygltf::Image& img, const tinygltf::Model& model, int id,
                                         const std::filesystem::path& cache_dir, const std::filesystem::path& in_dir)
{
    if (img.bufferView >= 0) {
        const tinygltf::BufferView& view = model.bufferViews[img.bufferView];
        const tinygltf::Buffer& buffer   = model.buffers[view.buffer];

        std::string extension = "." + tinygltf::MimeToExt(img.mimeType);

        std::filesystem::path path = cache_dir / ("_img_" + std::to_string(id) + extension);
        std::ofstream out(path.generic_u8string(), std::ios::binary);
        out.write(reinterpret_cast<const char*>(buffer.data.data() + view.byteOffset), view.byteLength);

        return path;
    } else if (!img.image.empty()) {
        std::string extension = "." + tinygltf::MimeToExt(img.mimeType);

        std::filesystem::path path = cache_dir / ("_img_" + std::to_string(id) + extension);
        std::ofstream out(path.generic_u8string(), std::ios::binary);
        out.write(reinterpret_cast<const char*>(img.image.data()), img.image.size());

        return path;
    } else {
        auto uri = std::filesystem::path(handleURI(img.uri));
        if (uri.is_absolute())
            return uri;
        else
            return std::filesystem::absolute(in_dir / uri);
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
    bool hasIndices = primitive.indices >= 0 && primitive.indices < (int)model.accessors.size();

    const tinygltf::Accessor* vertices = &model.accessors[primitive.attributes.at("POSITION")];
    const tinygltf::Accessor* normals  = hasNormal ? &model.accessors[primitive.attributes.at("NORMAL")] : nullptr;
    const tinygltf::Accessor* textures = hasTexture ? &model.accessors[primitive.attributes.at("TEXCOORD_0")] : nullptr;
    const tinygltf::Accessor* indices  = hasIndices ? &model.accessors[primitive.indices] : nullptr;

    if (vertices->type != TINYGLTF_TYPE_VEC3 || vertices->componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        IG_LOG(L_ERROR) << "glTF: Can not export mesh primitive " << path << " as it does contain an invalid POSITION attribute accessor" << std::endl;
        return;
    }

    if (indices && indices->type != TINYGLTF_TYPE_SCALAR) {
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

    const size_t triangleCount = indices ? indices->count / 3 : vertices->count / 3;
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

    if (indices) {
        const tinygltf::BufferView* indexBufferView = &model.bufferViews[indices->bufferView];
        const tinygltf::Buffer* indexBuffer         = &model.buffers[indexBufferView->buffer];
        const uint8* indexData                      = (indexBuffer->data.data() + indexBufferView->byteOffset + indices->byteOffset);

        for (size_t i = 0; i < triangleCount; ++i) {
            int byteStride    = indices->ByteStride(*indexBufferView);
            const uint8* p_i0 = indexData + byteStride * (3 * i + 0);
            const uint8* p_i1 = indexData + byteStride * (3 * i + 1);
            const uint8* p_i2 = indexData + byteStride * (3 * i + 2);

            int i0 = 0, i1 = 0, i2 = 0;
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
    } else {
        for (int i = 0; i < static_cast<int>(triangleCount); ++i) {
            int i0      = 3 * i + 0;
            int i1      = 3 * i + 1;
            int i2      = 3 * i + 2;
            uint8 count = 3;
            out.write(reinterpret_cast<const char*>(&count), sizeof(count));
            out.write(reinterpret_cast<const char*>(&i0), sizeof(i0));
            out.write(reinterpret_cast<const char*>(&i1), sizeof(i1));
            out.write(reinterpret_cast<const char*>(&i2), sizeof(i2));
        }
    }
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

#ifdef IG_GLTF_MAP_UNLIT_AS_LIGHT
inline static bool isMaterialUnlit(const tinygltf::Material& mat)
{
    return mat.extensions.count(KHR_materials_unlit.data()) > 0;
}
#endif

static std::string getTextureName(const tinygltf::Texture& tex)
{
    std::string name = "_tex" + std::to_string(tex.source);
    if (tex.sampler >= 0)
        name += "_" + std::to_string(tex.sampler);
    return name;
}

inline int getTextureIndex(const tinygltf::Value& val, const std::string& name)
{
    if (val.Has(name) && val.Get(name).IsObject()) {
        const auto& info = val.Get(name);
        if (info.Has("index") && info.Get("index").IsInt())
            return info.Get("index").GetNumberAsInt();
    }
    return -1;
}

template <int C>
inline Eigen::Matrix<float, C, 1> extractVector(const tinygltf::Value& parent, const std::string& name, const Eigen::Matrix<float, C, 1>& def)
{
    if (parent.Has(name) && parent.Get(name).IsArray()) {
        const auto& arr = parent.Get(name);

        if (arr.ArrayLen() == C) {
            Eigen::Matrix<float, C, 1> ret = def;
            for (int i = 0; i < C; ++i)
                ret[i] = (float)arr.Get(i).GetNumberAsDouble();
            return ret;
        }
    }

    return def;
}

inline Transformf getTextureTransformExts(const tinygltf::Value& t_ext)
{
    float rotation  = 0;
    Vector2f offset = extractVector<2>(t_ext, "offset", Vector2f::Zero());
    Vector2f scale  = extractVector<2>(t_ext, "scale", Vector2f::Ones());
    // Ignore texCoord

    if (t_ext.Has("rotation") && t_ext.Get("rotation").IsNumber())
        rotation = (float)t_ext.Get("rotation").GetNumberAsDouble();

    Transformf transform = Transformf::Identity();

    // Flip Y-UV back [x, 1-y] -> [x, y]
    transform.scale(Vector3f(1, -1, 1));
    transform.translate(Vector3f(0, -1, 0));

    transform.translate(Vector3f(offset.x(), offset.y(), 0));

    Matrix3f rotMat          = Matrix3f::Identity();
    rotMat.block<2, 2>(0, 0) = Eigen::Rotation2Df(-rotation).toRotationMatrix();
    transform.rotate(rotMat);

    transform.scale(Vector3f(scale.x(), scale.y(), 1));

    // Y-UV is flipped [x, y] -> [x, 1-y]
    transform.translate(Vector3f(0, 1, 0));
    transform.scale(Vector3f(1, -1, 1));

    return transform;
}

inline Transformf getTextureTransform(const tinygltf::Value& parent, const std::string& name, bool& hasOne)
{
    if (parent.Has(name) && parent.Get(name).IsObject()) {
        const auto& info = parent.Get(name);
        if (info.Has("extensions") && info.Get("extensions").IsObject()) {
            const auto& exts = info.Get("extensions");
            if (exts.Has(KHR_texture_transform.data()) && exts.Get(KHR_texture_transform.data()).IsObject()) {
                const auto& t_ext = exts.Get(KHR_texture_transform.data());
                hasOne            = true;
                return getTextureTransformExts(t_ext);
            }
        }
    }

    hasOne = false;
    return Transformf::Identity();
}

inline Transformf getTextureTransform(const tinygltf::TextureInfo& info, bool& hasOne)
{
    if (info.extensions.count(KHR_texture_transform.data())) {
        hasOne = true;
        return getTextureTransformExts(info.extensions.at(KHR_texture_transform.data()));
    } else {
        hasOne = false;
        return Transformf::Identity();
    }
}

std::string handleTexture(const tinygltf::TextureInfo& info, Scene& scene, const tinygltf::Model& model, const std::filesystem::path& directory)
{
    IG_ASSERT(info.index >= 0, "Expected valid texture info");

    const tinygltf::Texture& tex = model.textures[info.index];

    bool hasTransform    = false;
    Transformf transform = getTextureTransform(info, hasTransform);

    if (!hasTransform) {
        return getTextureName(tex);
    } else {
        const std::string original_tex = getTextureName(tex);
        auto obj                       = std::make_shared<Object>(OT_TEXTURE, "transform", directory);
        obj->setProperty("texture", Property::fromString(original_tex));
        obj->setProperty("transform", Property::fromTransform(transform));

        const std::string new_name = "__transform_tex_" + std::to_string(scene.textures().size());
        scene.addTexture(new_name, obj);
        return new_name;
    }
}

std::string handleTexture(const tinygltf::Value& parent, const std::string& name, Scene& scene, const tinygltf::Model& model, const std::filesystem::path& directory)
{
    int id = getTextureIndex(parent, name);
    if (id < 0)
        return "";

    const tinygltf::Texture& tex = model.textures[id];

    bool hasTransform    = false;
    Transformf transform = getTextureTransform(parent, name, hasTransform);

    if (!hasTransform) {
        return getTextureName(tex);
    } else {
        const std::string original_tex = getTextureName(tex);
        auto obj                       = std::make_shared<Object>(OT_TEXTURE, "transform", directory);
        obj->setProperty("texture", Property::fromString(original_tex));
        obj->setProperty("transform", Property::fromTransform(transform));

        const std::string new_name = "__transform_tex_" + std::to_string(scene.textures().size());
        scene.addTexture(new_name, obj);
        return new_name;
    }
}

static void addNodeMesh(Scene& scene, const tinygltf::Material& defaultMaterial, const std::filesystem::path& baseDir, const tinygltf::Model& model, const tinygltf::Node& node, const Transformf& transform)
{
    size_t primCount           = 0;
    const tinygltf::Mesh& mesh = model.meshes[node.mesh];
    for (const auto& prim : mesh.primitives) {
        const tinygltf::Material* material = nullptr;
        size_t mat_id                      = 0;
        if (prim.material < 0 || prim.material >= (int)model.materials.size()) {
            material = &defaultMaterial;
            mat_id   = model.materials.size() - 1; // Was appended to the end
        } else {
            material = &model.materials[prim.material];
            mat_id   = (size_t)prim.material;
        }

        const std::string name     = mesh.name + "_" + std::to_string(node.mesh) + "_" + std::to_string(primCount);
        const std::string bsdfName = getMaterialName(*material, mat_id);

        const bool hasMedium = material->extensions.count(KHR_materials_volume.data()) > 0; // TODO: Check if distance > 0

        auto obj = std::make_shared<Object>(OT_ENTITY, "", baseDir);
        obj->setProperty("shape", Property::fromString(name));
        obj->setProperty("bsdf", Property::fromString(bsdfName));
        if (hasMedium)
            obj->setProperty("inner_medium", Property::fromString(bsdfName)); // Shares the same name as the bsdf
        obj->setProperty("transform", Property::fromTransform(transform));

        const std::string entity_name = node.name + std::to_string(scene.entities().size()) + "_" + name;
        scene.addEntity(entity_name, obj);

        if (isMaterialEmissive(*material)) {
            auto light = std::make_shared<Object>(OT_LIGHT, "area", baseDir);
            light->setProperty("entity", Property::fromString(entity_name));

            float strength = 1;
            if (material->extensions.count(KHR_materials_emissive_strength.data())) {
                const auto& ext = material->extensions.at(KHR_materials_emissive_strength.data());
                if (ext.Has("emissiveStrength") && ext.Get("emissiveStrength").IsNumber()) {
                    strength = static_cast<float>(ext.Get("emissiveStrength").GetNumberAsDouble());
                }
            }

            if (material->emissiveTexture.index >= 0) {
                const std::string tex = handleTexture(material->emissiveTexture, scene, model, baseDir);
                light->setProperty("radiance", Property::fromString(tex + "*color("
                                                                    + std::to_string((float)material->emissiveFactor[0])
                                                                    + ", " + std::to_string((float)material->emissiveFactor[1])
                                                                    + ", " + std::to_string((float)material->emissiveFactor[2]) + ")"));
            } else {
                light->setProperty("radiance", Property::fromVector3(Vector3f((float)material->emissiveFactor[0], (float)material->emissiveFactor[1], (float)material->emissiveFactor[2]) * strength));
            }

            scene.addLight("_light_" + entity_name, light);
        }
#ifdef IG_GLTF_MAP_UNLIT_AS_LIGHT
        else if (isMaterialUnlit(*material)) {
            // Approximative unlit (which lits other parts)
            auto light = std::make_shared<Object>(OT_LIGHT, "area", baseDir);
            light->setProperty("entity", Property::fromString(entity_name));

            if (material->pbrMetallicRoughness.baseColorTexture.index >= 0) {
                const std::string tex = handleTexture(mat.pbrMetallicRoughness.baseColorTexture, scene, model, directory);
                bsdf->setProperty("radiance", Property::fromString(tex + "*color("
                                                                   + std::to_string((float)mat.pbrMetallicRoughness.baseColorFactor[0])
                                                                   + ", " + std::to_string((float)mat.pbrMetallicRoughness.baseColorFactor[1])
                                                                   + ", " + std::to_string((float)mat.pbrMetallicRoughness.baseColorFactor[2]) + ")"));
            } else {
                light->setProperty("radiance", Property::fromVector3(Vector3f((float)material->pbrMetallicRoughness.baseColorFactor[0], (float)material->pbrMetallicRoughness.baseColorFactor[1], (float)material->pbrMetallicRoughness.baseColorFactor[2])));
            }

            scene.addLight("_light_" + entity_name, light);
        }
#endif

        ++primCount;
    }
}

static void addNodeCamera(Scene& scene, const std::filesystem::path& baseDir, const tinygltf::Model& model, const tinygltf::Node& node, const Transformf& transform)
{
    if (scene.camera()) {
        IG_LOG(L_WARNING) << "glTF: No support for multiple cameras. Using first one" << std::endl;
        return;
    }

    Matrix3f rot;
    Matrix3f scale; // Ignore scale
    transform.computeRotationScaling(&rot, &scale);
    Vector3f trans = transform.translation();

    Transformf cameraTransform;
    cameraTransform.fromPositionOrientationScale(trans, rot, Vector3f::Ones());

    cameraTransform.scale(Vector3f(1, 1, -1)); // Flip -z to z

    const tinygltf::Camera& camera = model.cameras[node.camera];
    if (camera.type == "orthographic") {
        auto obj = std::make_shared<Object>(OT_CAMERA, "orthographic", baseDir);
        obj->setProperty("transform", Property::fromTransform(cameraTransform));
        obj->setProperty("near_clip", Property::fromNumber((float)camera.orthographic.znear));
        if (camera.orthographic.zfar > 0)
            obj->setProperty("far_clip", Property::fromNumber((float)camera.orthographic.zfar));
        obj->setProperty("scale", Property::fromNumber((float)camera.orthographic.xmag));
        obj->setProperty("aspect_ratio", Property::fromNumber(static_cast<float>(camera.orthographic.ymag / camera.orthographic.xmag)));
        scene.setCamera(obj);
    } else {
        auto obj = std::make_shared<Object>(OT_CAMERA, "perspective", baseDir);
        obj->setProperty("transform", Property::fromTransform(cameraTransform));
        obj->setProperty("vfov", Property::fromNumber((float)camera.perspective.yfov * Rad2Deg));
        obj->setProperty("near_clip", Property::fromNumber((float)camera.perspective.znear));
        if (camera.perspective.zfar > 0)
            obj->setProperty("far_clip", Property::fromNumber((float)camera.perspective.zfar));
        if (camera.perspective.aspectRatio > 0)
            obj->setProperty("aspect_ratio", Property::fromNumber((float)camera.perspective.aspectRatio));
        scene.setCamera(obj);
    }
}

static void addNodePunctualLight(Scene& scene, const std::filesystem::path& baseDir, const tinygltf::Model& model, const tinygltf::Node& node, const Transformf& transform)
{
    const auto& ext = node.extensions.at(KHR_lights_punctual.data());
    if (!ext.Has("light") || !ext.Get("light").IsInt())
        return;

    int lightID = ext.Get("light").GetNumberAsInt();
    if (lightID < 0 || lightID >= (int)model.lights.size())
        return;

    const auto& light = model.lights[lightID];

    Vector3f color = Vector3f::Ones() * light.intensity;
    if (light.color.size() == 3)
        color = Vector3f((float)light.color[0], (float)light.color[1], (float)light.color[2]) * (float)light.intensity;

    std::string type;
    if (light.type == "point") {
        auto obj = std::make_shared<Object>(OT_LIGHT, "point", baseDir);
        obj->setProperty("position", Property::fromVector3(transform * Vector3f::Zero()));
        obj->setProperty("intensity", Property::fromVector3(color));
        scene.addLight("_l_" + std::to_string(scene.lights().size()), obj);
    } else if (light.type == "spot") {
        Vector3f dir = (transform.linear().inverse().transpose() * Vector3f(0.0f, 0.0f, -1.0f)).normalized();
        auto obj     = std::make_shared<Object>(OT_LIGHT, "spot", baseDir);
        obj->setProperty("position", Property::fromVector3(transform * Vector3f::Zero()));
        obj->setProperty("direction", Property::fromVector3(dir));
        obj->setProperty("intensity", Property::fromVector3(color));
        obj->setProperty("cutoff", Property::fromNumber(light.spot.outerConeAngle * Rad2Deg));
        obj->setProperty("falloff", Property::fromNumber(light.spot.innerConeAngle * Rad2Deg));
        scene.addLight("_l_" + std::to_string(scene.lights().size()), obj);
    } else if (light.type == "directional") {
        Vector3f dir = (transform.linear().inverse().transpose() * Vector3f(0.0f, 0.0f, -1.0f)).normalized();
        auto obj     = std::make_shared<Object>(OT_LIGHT, "directional", baseDir);
        obj->setProperty("direction", Property::fromVector3(dir));
        obj->setProperty("irradiance", Property::fromVector3(color));
        scene.addLight("_l_" + std::to_string(scene.lights().size()), obj);
    } else {
        IG_LOG(L_ERROR) << "Unknown glTF punctual light type '" << light.type << "'" << std::endl;
    }
}

static void addNode(Scene& scene, const tinygltf::Material& defaultMaterial, const std::filesystem::path& baseDir, const tinygltf::Model& model, const tinygltf::Node& node, const Transformf& parent)
{
    Transformf transform = parent;
    if (node.matrix.size() == 16)
        transform *= Eigen::Map<Eigen::Matrix4d>(const_cast<double*>(node.matrix.data())).cast<float>();

    if (node.translation.size() == 3)
        transform.translate(Vector3f((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]));

    if (node.rotation.size() == 4)
        transform.rotate(Quaternionf((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]));

    if (node.scale.size() == 3)
        transform.scale(Vector3f((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]));

    if (node.mesh >= 0)
        addNodeMesh(scene, defaultMaterial, baseDir, model, node, transform);

    if (node.camera >= 0)
        addNodeCamera(scene, baseDir, model, node, transform);

    if (node.extensions.count(KHR_lights_punctual.data()) > 0)
        addNodePunctualLight(scene, baseDir, model, node, transform);

    for (int child : node.children)
        addNode(scene, defaultMaterial, baseDir, model, model.nodes[child], transform);
}

static void loadTextures(Scene& scene, const tinygltf::Model& model, const std::filesystem::path& directory, const std::filesystem::path& cache_dir)
{
    std::unordered_map<int, std::filesystem::path> loaded_images;
    for (const auto& tex : model.textures) {
        std::filesystem::path img_path;
        if (loaded_images.count(tex.source) > 0) {
            img_path = loaded_images[tex.source];
        } else {
            const tinygltf::Image& img = model.images[tex.source];
            img_path                   = exportImage(img, model, tex.source, cache_dir / "images", directory);

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
}

static void loadMaterials(Scene& scene, const tinygltf::Model& model, const std::filesystem::path& directory)
{
    size_t matCounter = 0;
    for (const auto& mat : model.materials) {
        std::string name = getMaterialName(mat, matCounter);
        auto bsdf        = std::make_shared<Object>(OT_BSDF, "principled", directory);

        if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
            const std::string tex = handleTexture(mat.pbrMetallicRoughness.baseColorTexture, scene, model, directory);
            bsdf->setProperty("base_color", Property::fromString(tex + "*color("
                                                                 + std::to_string((float)mat.pbrMetallicRoughness.baseColorFactor[0])
                                                                 + ", " + std::to_string((float)mat.pbrMetallicRoughness.baseColorFactor[1])
                                                                 + ", " + std::to_string((float)mat.pbrMetallicRoughness.baseColorFactor[2]) + ")"));
        } else {
            bsdf->setProperty("base_color", Property::fromVector3(Vector3f((float)mat.pbrMetallicRoughness.baseColorFactor[0], (float)mat.pbrMetallicRoughness.baseColorFactor[1], (float)mat.pbrMetallicRoughness.baseColorFactor[2])));
        }

        if (mat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
            const std::string mrtex = handleTexture(mat.pbrMetallicRoughness.metallicRoughnessTexture, scene, model, directory);
            bsdf->setProperty("metallic", Property::fromString(mrtex + ".b*" + std::to_string((float)mat.pbrMetallicRoughness.metallicFactor)));
            bsdf->setProperty("roughness", Property::fromString(mrtex + ".g*" + std::to_string((float)mat.pbrMetallicRoughness.roughnessFactor)));
        } else {
            bsdf->setProperty("metallic", Property::fromNumber((float)mat.pbrMetallicRoughness.metallicFactor));
            bsdf->setProperty("roughness", Property::fromNumber((float)mat.pbrMetallicRoughness.roughnessFactor));
        }

        // Extensions
        if (mat.extensions.count(KHR_materials_ior.data()) > 0) {
            const auto& ext = mat.extensions.at(KHR_materials_ior.data());
            if (ext.Has("ior") && ext.Get("ior").IsNumber())
                bsdf->setProperty("ior", Property::fromNumber((float)ext.Get("ior").GetNumberAsDouble()));
        } else {
            bsdf->setProperty("ior", Property::fromNumber(1.5));
        }

        if (mat.extensions.count(KHR_materials_sheen.data()) > 0) {
            const auto& ext       = mat.extensions.at(KHR_materials_sheen.data());
            const std::string tex = handleTexture(ext, "sheenColorTexture", scene, model, directory);

            float factor = 0;
            if (ext.Has("sheenColorFactor") && ext.Get("sheenColorFactor").IsNumber())
                factor = static_cast<float>(ext.Get("sheenColorFactor").GetNumberAsDouble());

            // No support for colored sheen
            if (!tex.empty())
                bsdf->setProperty("sheen", Property::fromString("avg(" + tex + ")*" + std::to_string(factor)));
            else
                bsdf->setProperty("sheen", Property::fromNumber(factor));
        }

        // Only support for transmissionFactor & transmissionTexture
        if (mat.extensions.count(KHR_materials_transmission.data()) > 0) {
            const auto& ext       = mat.extensions.at(KHR_materials_transmission.data());
            const std::string tex = handleTexture(ext, "transmissionTexture", scene, model, directory);

            float factor = 0;
            if (ext.Has("transmissionFactor") && ext.Get("transmissionFactor").IsNumber())
                factor = static_cast<float>(ext.Get("transmissionFactor").GetNumberAsDouble());

            if (!tex.empty())
                bsdf->setProperty("specular_transmission", Property::fromString(tex + ".r*" + std::to_string(factor)));
            else
                bsdf->setProperty("specular_transmission", Property::fromNumber(factor));

            bool is_thin = true;
            if (mat.extensions.count(KHR_materials_volume.data()) > 0) {
                const auto& ext2 = mat.extensions.at(KHR_materials_volume.data());

                // TODO: No support for textures
                float thickness = 0;
                if (ext2.Has("thicknessFactor") && ext2.Get("thicknessFactor").IsNumber())
                    thickness = static_cast<float>(ext2.Get("thicknessFactor").GetNumberAsDouble());
                is_thin = thickness <= FltEps;
            }

            bsdf->setProperty("thin", Property::fromBool(is_thin));
        }

        // Not ratified yet, but who cares
        if (mat.extensions.count(KHR_materials_translucency.data()) > 0) {
            const auto& ext       = mat.extensions.at(KHR_materials_translucency.data());
            const std::string tex = handleTexture(ext, "translucencyTexture", scene, model, directory);

            float factor = 0;
            if (ext.Has("translucencyFactor") && ext.Get("translucencyFactor").IsNumber())
                factor = static_cast<float>(ext.Get("translucencyFactor").GetNumberAsDouble());

            if (!tex.empty())
                bsdf->setProperty("diffuse_transmission", Property::fromString(tex + ".r*" + std::to_string(factor)));
            else
                bsdf->setProperty("diffuse_transmission", Property::fromNumber(factor));
        }

        // No support for thickness as this is a raytracer
        if (mat.extensions.count(KHR_materials_volume.data()) > 0) {
            const auto& ext = mat.extensions.at(KHR_materials_volume.data());

            float thickness = 0;
            if (ext.Has("thicknessFactor") && ext.Get("thicknessFactor").IsNumber())
                thickness = static_cast<float>(ext.Get("thicknessFactor").GetNumberAsDouble());

            float distance = std::numeric_limits<float>::infinity();
            if (ext.Has("attenuationDistance") && ext.Get("attenuationDistance").IsNumber())
                distance = static_cast<float>(ext.Get("attenuationDistance").GetNumberAsDouble());

            if (distance > FltEps && thickness > FltEps) {
                Vector3f color   = extractVector<3>(ext, "attenuationColor", Vector3f::Ones());
                Vector3f sigma_a = -color.array().log() / distance;
                auto medium      = std::make_shared<Object>(OT_MEDIUM, "homogeneous", directory);
                medium->setProperty("sigma_s", Property::fromVector3(Vector3f::Zero()));
                medium->setProperty("sigma_a", Property::fromVector3(sigma_a));

                scene.addMedium(name, medium);
            }
        }

        // No support for clearcoatNormalTexture
        if (mat.extensions.count(KHR_materials_clearcoat.data()) > 0) {
            const auto& ext       = mat.extensions.at(KHR_materials_clearcoat.data());
            const std::string tex = handleTexture(ext, "clearcoatTexture", scene, model, directory);

            float factor = 0;
            if (ext.Has("clearcoatFactor") && ext.Get("clearcoatFactor").IsNumber())
                factor = static_cast<float>(ext.Get("clearcoatFactor").GetNumberAsDouble());

            if (!tex.empty())
                bsdf->setProperty("clearcoat", Property::fromString(tex + ".r*" + std::to_string(factor)));
            else
                bsdf->setProperty("clearcoat", Property::fromNumber(factor));

            const std::string rtex = handleTexture(ext, "clearcoatRoughnessTexture", scene, model, directory);
            float rfactor          = 0;
            if (ext.Has("clearcoatRoughnessFactor") && ext.Get("clearcoatRoughnessFactor").IsNumber())
                rfactor = static_cast<float>(ext.Get("clearcoatRoughnessFactor").GetNumberAsDouble());

            if (!rtex.empty())
                bsdf->setProperty("clearcoat_roughness", Property::fromString(rtex + ".g*" + std::to_string(rfactor)));
            else
                bsdf->setProperty("clearcoat_roughness", Property::fromNumber(rfactor));

            bsdf->setProperty("clearcoat_top_only", Property::fromBool(!mat.doubleSided));
            // TODO: Apply factor to emission for "darkening" by the cosine term
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

            auto factor = static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[3]);
            if (factor > 0 && mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                bsdf->setProperty("weight", Property::fromString(handleTexture(mat.pbrMetallicRoughness.baseColorTexture, scene, model, directory) + ".a"));
                bsdf->setProperty("cutoff", Property::fromNumber((float)mat.alphaCutoff / factor));
            } else {
                bsdf->setProperty("weight", Property::fromNumber(factor));
                bsdf->setProperty("cutoff", Property::fromNumber((float)mat.alphaCutoff));
            }
        } else if (mat.alphaMode == "BLEND") {
            scene.addBSDF(name + "_blend_inner", bsdf);

            bsdf = std::make_shared<Object>(OT_BSDF, "mask", directory);
            bsdf->setProperty("bsdf", Property::fromString(name + "_blend_inner"));
            bsdf->setProperty("inverted", Property::fromBool(true));

            auto factor = static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[3]);
            if (factor > 0 && mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                bsdf->setProperty("weight", Property::fromString(handleTexture(mat.pbrMetallicRoughness.baseColorTexture, scene, model, directory) + ".a"));
            } else {
                bsdf->setProperty("weight", Property::fromNumber(factor));
            }
        }

        scene.addBSDF(name, bsdf);
        ++matCounter;
    }
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
    loadTextures(scene, model, directory, cache_dir);

    tinygltf::Material defaultMaterial;
    tinygltf::ParseMaterial(&defaultMaterial, nullptr, {}, false);
    model.materials.push_back(defaultMaterial);

    loadMaterials(scene, model, directory);

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
        addNode(scene, defaultMaterial, directory, model, model.nodes[nodeId], Transformf::Identity());

    return scene;
}
} // namespace IG::Parser