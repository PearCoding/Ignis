#include "Parser.h"
#include "Logger.h"
#include "StringUtils.h"
#include "math/Tangent.h"

#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/istreamwrapper.h>

#include "glTFParser.h"

namespace IG {
using namespace std::string_view_literals;

// TODO: (Re)Add arguments!

using Arguments = std::unordered_map<std::string, std::string>;

// ------------- File IO
static inline bool doesFileExist(const Path& fileName)
{
    return std::ifstream(fileName.generic_string()).good();
}

static inline Path resolvePath(const Path& path, const Path& baseDir)
{
    if (doesFileExist(baseDir / path))
        return baseDir / path;

    if (doesFileExist(path))
        return path;
    else
        return {};
}

// ------------- Parser

inline static std::string getString(const rapidjson::Value& obj)
{
    return std::string(obj.GetString(), obj.GetStringLength());
}

inline bool checkArrayIsAllNumber(const rapidjson::GenericArray<true, rapidjson::Value>& arr)
{
    if (arr.Size() == 0)
        return false;
    for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
        if (!arr[i].IsNumber())
            return false;
    }
    return true;
}

inline bool checkArrayIsAllInteger(const rapidjson::GenericArray<true, rapidjson::Value>& arr)
{
    if (arr.Size() == 0)
        return false;
    for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
        if (!arr[i].IsInt())
            return false;
    }
    return true;
}

inline static Vector2f getVector2f(const rapidjson::Value& obj)
{
    const auto& array = obj.GetArray();
    const size_t len  = array.Size();
    if (len != 2)
        throw std::runtime_error("Expected vector of length 2");
    if (!checkArrayIsAllNumber(array))
        throw std::runtime_error("Given vector is not only numbers");
    return Vector2f(array[0].GetFloat(), array[1].GetFloat());
}

inline static Vector3f getVector3f(const rapidjson::Value& obj, bool allow2d = false)
{
    const auto& array = obj.GetArray();
    const size_t len  = array.Size();
    if (allow2d && len == 2) {
        const Vector2f var = getVector2f(obj);
        return Vector3f(var.x(), var.y(), 0);
    }

    if (len != 3)
        throw std::runtime_error("Expected vector of length 3");
    if (!checkArrayIsAllNumber(array))
        throw std::runtime_error("Given vector is not only numbers");
    return Vector3f(array[0].GetFloat(), array[1].GetFloat(), array[2].GetFloat());
}

// [w, x, y, z]
inline static Quaternionf getQuaternionf(const rapidjson::Value& obj)
{
    const auto& array = obj.GetArray();
    const size_t len  = array.Size();
    if (len != 4)
        throw std::runtime_error("Expected vector of length 4");
    if (!checkArrayIsAllNumber(array))
        throw std::runtime_error("Given vector is not only numbers");
    return Quaternionf(array[0].GetFloat(), array[1].GetFloat(), array[2].GetFloat(), array[3].GetFloat());
}

inline static Matrix3f getMatrix3f(const rapidjson::Value& obj)
{
    const auto& array = obj.GetArray();
    const size_t len  = array.Size();
    if (len != 9)
        throw std::runtime_error("Expected matrix of length 9 (3x3)");
    if (!checkArrayIsAllNumber(array))
        throw std::runtime_error("Given matrix is not only numbers");

    // Eigen uses column major by default. Our input is row major!
    Matrix3f mat;
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j)
            mat(i, j) = array[rapidjson::SizeType(i * 3 + j)].GetFloat();
    return mat;
}

inline static Matrix4f getMatrix4f(const rapidjson::Value& obj)
{
    const auto& array = obj.GetArray();
    const size_t len  = array.Size();
    const size_t rows = len == 12 ? 3 : 4;
    if (len != 16 && len != 12)
        throw std::runtime_error("Expected matrix of length 16 (4x4) or length 12 (3x4)");
    if (!checkArrayIsAllNumber(array))
        throw std::runtime_error("Given matrix is not only numbers");

    Matrix4f mat = Matrix4f::Identity();
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < 4; ++j)
            mat(i, j) = array[rapidjson::SizeType(i * 4 + j)].GetFloat();
    return mat;
}

template <typename Derived1, typename Derived2, typename Derived3>
Eigen::Matrix<typename Derived1::Scalar, 3, 4> lookAt(const Eigen::MatrixBase<Derived1>& eye,
                                                      const Eigen::MatrixBase<Derived2>& center,
                                                      const Eigen::MatrixBase<Derived3>& up)
{
    using Vector3  = Eigen::Matrix<typename Derived1::Scalar, 3, 1>;
    using Matrix34 = Eigen::Matrix<typename Derived1::Scalar, 3, 4>;

    Vector3 f = (center - eye).normalized();
    if (f.squaredNorm() <= FltEps)
        f = Vector3::UnitZ();

    Vector3 u = up.normalized();
    Vector3 s = f.cross(u).normalized();
    u         = s.cross(f);

    if (u.squaredNorm() <= FltEps) {
        Tangent::frame(f, s, u);
    }

    Matrix34 m;
    m.col(0) = s;
    m.col(1) = u;
    m.col(2) = f;
    m.col(3) = eye;

    return m;
}

inline static void applyTransformProperty(Transformf& transform, const rapidjson::GenericObject<true, rapidjson::Value>& obj)
{
    // For legacy purposes, multiple operations can defined.
    // New scenes should only use a single operation and concat it one level above.

    // From left to right. The last entry will be applied first to a potential point A1*A2*A3*...*An*p
    for (auto val = obj.MemberBegin(); val != obj.MemberEnd(); ++val) {
        if (val->name == "translate") {
            const Vector3f pos = getVector3f(val->value, true);
            transform.translate(pos);
        } else if (val->name == "scale") {
            if (val->value.IsNumber()) {
                transform.scale(val->value.GetFloat());
            } else {
                const Vector3f s = getVector3f(val->value, true);
                transform.scale(s);
            }
        } else if (val->name == "rotate") { // [Rotation around X, Rotation around Y, Rotation around Z] all in degrees
            const Vector3f angles = getVector3f(val->value, true);
            transform *= Eigen::AngleAxisf(Deg2Rad * angles(0), Vector3f::UnitX())
                         * Eigen::AngleAxisf(Deg2Rad * angles(1), Vector3f::UnitY())
                         * Eigen::AngleAxisf(Deg2Rad * angles(2), Vector3f::UnitZ());
        } else if (val->name == "qrotate") { // 4D Vector quaternion
            transform *= getQuaternionf(val->value);
        } else if (val->name == "lookat") {
            if (val->value.IsObject()) {
                Vector3f origin = Vector3f::Zero();
                Vector3f target = Vector3f::UnitY();
                Vector3f up     = Vector3f::UnitZ();

                std::optional<Vector3f> direction;
                for (auto val2 = val->value.MemberBegin(); val2 != val->value.MemberEnd(); ++val2) {
                    if (val2->name == "origin")
                        origin = getVector3f(val2->value);
                    else if (val2->name == "target")
                        target = getVector3f(val2->value);
                    else if (val2->name == "up")
                        up = getVector3f(val2->value);
                    else if (val2->name == "direction") // You might give the direction instead of the target
                        direction = getVector3f(val2->value);
                }

                if (direction.has_value())
                    transform *= lookAt(origin, direction.value() + origin, up);
                else
                    transform *= lookAt(origin, target, up);
            } else
                throw std::runtime_error("Expected transform lookat property to be an object with origin, target and optional up vector");
        } else if (val->name == "matrix") {
            const size_t len = val->value.GetArray().Size();
            if (len == 9)
                transform = transform * Transformf(getMatrix3f(val->value));
            else if (len == 12 || len == 16)
                transform = transform * Transformf(getMatrix4f(val->value));
            else
                throw std::runtime_error("Expected transform property to be an array of size 9 or 16");
        } else {
            throw std::runtime_error("Transform property got unknown entry type '" + std::string(val->name.GetString()) + "'");
        }
    }
}

inline static Transformf getTransform(const rapidjson::GenericArray<true, rapidjson::Value>& array)
{
    Transformf transform = Transformf::Identity();
    for (rapidjson::SizeType i = 0; i < array.Size(); ++i) {
        if (!array[i].IsObject())
            throw std::runtime_error("Given transform property does not consists of transform operations only");

        const auto obj = array[i].GetObject();
        applyTransformProperty(transform, obj);
    }
    return transform;
}

inline static SceneProperty handleArrayProperty(const rapidjson::Value& obj)
{
    const auto values_it = obj.FindMember("values");
    const auto type_it   = obj.FindMember("type");

    bool assumeNumbers = true;
    if (type_it != obj.MemberEnd() && type_it->value.IsString() && (type_it->value == "int" || type_it->value == "integer"))
        assumeNumbers = false;

    IG_ASSERT(values_it != obj.MemberEnd(), "Expected 'values' to be present");

    if (!values_it->value.IsArray())
        throw std::runtime_error("Expected array property to have an array 'values' member");

    const auto arr = values_it->value.GetArray();
    if (assumeNumbers) {
        if (!checkArrayIsAllNumber(arr))
            throw std::runtime_error("Expected number array to have only numbers in it");

        SceneProperty::NumberArray values;
        values.reserve(arr.Size());
        for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
            values.push_back(arr[i].GetFloat());
        return SceneProperty::fromNumberArray(std::move(values));
    } else {
        if (!checkArrayIsAllInteger(arr))
            throw std::runtime_error("Expected integer array to have only integers in it");

        SceneProperty::IntegerArray values;
        values.reserve(arr.Size());
        for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
            values.push_back(arr[i].GetInt());
        return SceneProperty::fromIntegerArray(std::move(values));
    }
}

inline static SceneProperty getProperty(const rapidjson::Value& obj)
{
    if (obj.IsBool())
        return SceneProperty::fromBool(obj.GetBool());
    else if (obj.IsString())
        return SceneProperty::fromString(getString(obj));
    else if (obj.IsInt())
        return SceneProperty::fromInteger(obj.GetInt());
    else if (obj.IsNumber())
        return SceneProperty::fromNumber(obj.GetFloat());
    else if (obj.IsArray()) {
        const auto& array = obj.GetArray();
        const size_t len  = array.Size();

        if (len >= 1 && array[0].IsObject())
            return SceneProperty::fromTransform(getTransform(array));
        else if (len == 2)
            return SceneProperty::fromVector2(getVector2f(obj));
        else if (len == 3)
            return SceneProperty::fromVector3(getVector3f(obj));
        else if (len == 9)
            return SceneProperty::fromTransform(Transformf(getMatrix3f(obj)));
        else if (len == 12 || len == 16)
            return SceneProperty::fromTransform(Transformf(getMatrix4f(obj)));
    } else if (obj.IsObject()) {
        const auto innerObj = obj.GetObject();
        if (innerObj.HasMember("values")) {
            return handleArrayProperty(innerObj);
        } else {
            // Deprecated way of handling transforms
            Transformf transform = Transformf::Identity();

            applyTransformProperty(transform, innerObj);
            return SceneProperty::fromTransform(transform);
        }
    }

    return SceneProperty();
}

inline static void populateObject(std::shared_ptr<SceneObject>& ptr, const rapidjson::Value& obj)
{
    for (auto itr = obj.MemberBegin(); itr != obj.MemberEnd(); ++itr) {
        const std::string name = getString(itr->name);

        // Skip name and type entries
        if (name == "name"sv || name == "type"sv)
            continue;

        const auto prop = getProperty(itr->value);
        if (prop.isValid())
            ptr->setProperty(name, prop);
    }
}

static std::shared_ptr<SceneObject> handleAnonymousObject(Scene& scene, SceneObject::Type type, const Path& baseDir, const rapidjson::Value& obj)
{
    IG_UNUSED(scene);

    if (obj.HasMember("type") && !obj["type"].IsString())
        throw std::runtime_error("Expected type to be a string");

    const auto pluginType = obj.HasMember("type") ? getString(obj["type"]) : "";

    auto ptr = std::make_shared<SceneObject>(type, pluginType, baseDir);
    populateObject(ptr, obj);
    return ptr;
}

static void handleNamedObject(Scene& scene, SceneObject::Type type, const Path& baseDir, const rapidjson::Value& obj)
{
    if (obj.HasMember("type") && !obj["type"].IsString())
        throw std::runtime_error("Expected type to be a string");

    if (!obj.HasMember("name"))
        throw std::runtime_error("Expected name");

    if (!obj["name"].IsString())
        throw std::runtime_error("Expected name to be a string");

    const auto pluginType = obj.HasMember("type") ? getString(obj["type"]) : "";
    const auto name       = getString(obj["name"]);

    auto ptr = std::make_shared<SceneObject>(type, pluginType, baseDir);
    populateObject(ptr, obj);

    switch (type) {
    case SceneObject::OT_SHAPE:
        scene.addShape(name, ptr);
        break;
    case SceneObject::OT_TEXTURE:
        scene.addTexture(name, ptr);
        break;
    case SceneObject::OT_BSDF:
        scene.addBSDF(name, ptr);
        break;
    case SceneObject::OT_LIGHT:
        scene.addLight(name, ptr);
        break;
    case SceneObject::OT_MEDIUM:
        scene.addMedium(name, ptr);
        break;
    case SceneObject::OT_ENTITY:
        scene.addEntity(name, ptr);
        break;
    case SceneObject::OT_PARAMETER:
        scene.addParameter(name, ptr);
        break;
    default:
        IG_ASSERT(false, "Unknown object type");
        break;
    }
}

static void handleExternalObject(SceneParser& loader, Scene& scene, const Path& baseDir, const rapidjson::Value& obj, uint32 flags)
{
    if (obj.HasMember("type") && !obj["type"].IsString())
        throw std::runtime_error("Expected type to be a string");

    if (!obj.HasMember("filename"))
        throw std::runtime_error("Expected a path for externals");

    std::string pluginType     = obj.HasMember("type") ? to_lowercase(getString(obj["type"])) : "";
    const std::string inc_path = getString(obj["filename"]);
    const Path path            = std::filesystem::canonical(resolvePath(inc_path, baseDir));
    if (path.empty())
        throw std::runtime_error("Could not find path '" + inc_path + "'");

    // If type is empty, determine type by file extension
    if (pluginType.empty()) {
        if (to_lowercase(path.extension().generic_string()) == ".json"sv)
            pluginType = "ignis";
        else if (to_lowercase(path.extension().generic_string()) == ".gltf"sv || to_lowercase(path.extension().generic_string()) == ".glb"sv)
            pluginType = "gltf";
        else
            throw std::runtime_error("Could not determine external type by filename '" + path.generic_string() + "'");
    }

    if (pluginType == "ignis"sv) {
        // Include ignis file
        auto local_scene = loader.loadFromFile(path, flags);

        if (local_scene == nullptr)
            throw std::runtime_error("Could not load '" + path.generic_string() + "'");

        scene.addFrom(*local_scene);
    } else if (pluginType == "gltf"sv) {
        // Include and map gltf stuff
        auto local_scene = glTFSceneParser::loadFromFile(path); // TODO: Maybe honor flags?

        if (local_scene == nullptr)
            throw std::runtime_error("Could not load '" + path.generic_string() + "'");

        scene.addFrom(*local_scene);
    } else {
        throw std::runtime_error("Unknown external type '" + pluginType + "' given");
    }
}

#define _CHECK_FLAG(x) ((flags & x) == x)
class InternalSceneParser {
public:
    static std::shared_ptr<Scene> loadFromJSON(SceneParser& loader, const Path& baseDir, const rapidjson::Document& doc, uint32 flags)
    {
        if (!doc.IsObject())
            throw std::runtime_error("Expected root element to be an object");

        std::shared_ptr<Scene> scene = std::make_shared<Scene>();

        // Handle externals first, such that the current file can replace parts of it later on
        if (_CHECK_FLAG(SceneParser::F_LoadExternals) && doc.HasMember("externals")) {
            if (!doc["externals"].IsArray())
                throw std::runtime_error("Expected 'external' elements to be an array");
            for (const auto& exts : doc["externals"].GetArray()) {
                if (!exts.IsObject())
                    throw std::runtime_error("Expected 'external' element to be an object");

                handleExternalObject(loader, *scene, baseDir, exts, flags);
            }
        }

        if (_CHECK_FLAG(SceneParser::F_LoadCamera) && doc.HasMember("camera"))
            scene->setCamera(handleAnonymousObject(*scene, SceneObject::OT_CAMERA, baseDir, doc["camera"]));

        if (_CHECK_FLAG(SceneParser::F_LoadTechnique) && doc.HasMember("technique"))
            scene->setTechnique(handleAnonymousObject(*scene, SceneObject::OT_TECHNIQUE, baseDir, doc["technique"]));

        if (_CHECK_FLAG(SceneParser::F_LoadFilm) && doc.HasMember("film"))
            scene->setFilm(handleAnonymousObject(*scene, SceneObject::OT_FILM, baseDir, doc["film"]));

        if (_CHECK_FLAG(SceneParser::F_LoadShapes) && doc.HasMember("shapes")) {
            if (!doc["shapes"].IsArray())
                throw std::runtime_error("Expected 'shapes' element to be an array");
            for (const auto& shape : doc["shapes"].GetArray()) {
                if (!shape.IsObject())
                    throw std::runtime_error("Expected 'shape' element to be an object");
                handleNamedObject(*scene, SceneObject::OT_SHAPE, baseDir, shape);
            }
        }

        if (_CHECK_FLAG(SceneParser::F_LoadTextures) && doc.HasMember("textures")) {
            if (!doc["textures"].IsArray())
                throw std::runtime_error("Expected 'textures' element to be an array");
            for (const auto& tex : doc["textures"].GetArray()) {
                if (!tex.IsObject())
                    throw std::runtime_error("Expected 'texture' element to be an object");
                handleNamedObject(*scene, SceneObject::OT_TEXTURE, baseDir, tex);
            }
        }

        if (_CHECK_FLAG(SceneParser::F_LoadBSDFs) && doc.HasMember("bsdfs")) {
            if (!doc["bsdfs"].IsArray())
                throw std::runtime_error("Expected 'bsdfs' element to be an array");
            for (const auto& bsdf : doc["bsdfs"].GetArray()) {
                if (!bsdf.IsObject())
                    throw std::runtime_error("Expected 'bsdf' element to be an object");
                handleNamedObject(*scene, SceneObject::OT_BSDF, baseDir, bsdf);
            }
        }

        if (_CHECK_FLAG(SceneParser::F_LoadLights) && doc.HasMember("lights")) {
            if (!doc["lights"].IsArray())
                throw std::runtime_error("Expected 'lights' element to be an array");
            for (const auto& light : doc["lights"].GetArray()) {
                if (!light.IsObject())
                    throw std::runtime_error("Expected 'light' element to be an object");
                handleNamedObject(*scene, SceneObject::OT_LIGHT, baseDir, light);
            }
        }

        if (_CHECK_FLAG(SceneParser::F_LoadMedia) && doc.HasMember("media")) {
            if (!doc["media"].IsArray())
                throw std::runtime_error("Expected 'media' element to be an array");
            for (const auto& medium : doc["media"].GetArray()) {
                if (!medium.IsObject())
                    throw std::runtime_error("Expected 'medium' element to be an object");
                handleNamedObject(*scene, SceneObject::OT_MEDIUM, baseDir, medium);
            }
        }

        if (_CHECK_FLAG(SceneParser::F_LoadEntities) && doc.HasMember("entities")) {
            if (!doc["entities"].IsArray())
                throw std::runtime_error("Expected 'entities' element to be an array");
            for (const auto& entity : doc["entities"].GetArray()) {
                if (!entity.IsObject())
                    throw std::runtime_error("Expected 'entity' element to be an object");
                handleNamedObject(*scene, SceneObject::OT_ENTITY, baseDir, entity);
            }
        }

        if (_CHECK_FLAG(SceneParser::F_LoadParameters) && doc.HasMember("parameters")) {
            if (!doc["parameters"].IsArray())
                throw std::runtime_error("Expected 'parameters' element to be an array");
            for (const auto& param : doc["parameters"].GetArray()) {
                if (!param.IsObject())
                    throw std::runtime_error("Expected 'parameter' element to be an object");
                handleNamedObject(*scene, SceneObject::OT_PARAMETER, baseDir, param);
            }
        }

        return scene;
    }
};
#undef _CHECK_FLAG

constexpr auto JsonFlags = rapidjson::kParseDefaultFlags | rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag | rapidjson::kParseNanAndInfFlag | rapidjson::kParseEscapedApostropheFlag;

std::shared_ptr<Scene> SceneParser::loadFromFile(const Path& path, uint32 flags)
{
    if (path.extension() == ".gltf"sv || path.extension() == ".glb"sv) {
        // Load gltf directly
        std::shared_ptr<Scene> scene = glTFSceneParser::loadFromFile(path);
        if (scene) {
            // Scene is using volumes, switch to the volpath technique
            if (!scene->media().empty())
                scene->setTechnique(std::make_shared<SceneObject>(SceneObject::OT_TECHNIQUE, "volpath", path.parent_path()));

            // Add light to the scene if no light is available (preview style)
            if (scene->lights().empty()) {
                IG_LOG(L_WARNING) << "No lights available in " << path << ". Adding default environment light" << std::endl;
                scene->addConstantEnvLight();
            }
        }
        return scene;
    }

    std::ifstream ifs(path.generic_string());
    if (!ifs.good()) {
        IG_LOG(L_ERROR) << "Could not open file '" << path << "'" << std::endl;
        return nullptr;
    }

    rapidjson::IStreamWrapper isw(ifs);

    rapidjson::Document doc;
    if (doc.ParseStream<JsonFlags>(isw).HasParseError()) {
        IG_LOG(L_ERROR) << "JSON[" << doc.GetErrorOffset() << "]: " << rapidjson::GetParseError_En(doc.GetParseError()) << std::endl;
        return nullptr;
    }

    const Path parent = path.has_parent_path() ? std::filesystem::canonical(path.parent_path()) : Path{};
    return InternalSceneParser::loadFromJSON(*this, parent, doc, flags);
}

std::shared_ptr<Scene> SceneParser::loadFromString(const std::string& str, const Path& opt_dir, uint32 flags)
{
    rapidjson::Document doc;
    if (doc.Parse<JsonFlags>(str.c_str()).HasParseError()) {
        IG_LOG(L_ERROR) << "JSON[" << doc.GetErrorOffset() << "]: " << rapidjson::GetParseError_En(doc.GetParseError()) << std::endl;
        return nullptr;
    }
    return InternalSceneParser::loadFromJSON(*this, opt_dir, doc, flags);
}

} // namespace IG