#include "Parser.h"
#include "Logger.h"
#include "StringUtils.h"
#include "math/Tangent.h"

#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/istreamwrapper.h>

#include "glTFParser.h"

namespace IG::Parser {
// TODO: (Re)Add arguments!

using Arguments = std::unordered_map<std::string, std::string>;

// ------------- File IO
static inline bool doesFileExist(const std::filesystem::path& fileName)
{
    return std::ifstream(fileName.generic_u8string()).good();
}

static inline std::filesystem::path resolvePath(const std::filesystem::path& path, const std::filesystem::path& baseDir)
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

inline static Property getProperty(const rapidjson::Value& obj)
{
    if (obj.IsBool())
        return Property::fromBool(obj.GetBool());
    else if (obj.IsString())
        return Property::fromString(getString(obj));
    else if (obj.IsInt())
        return Property::fromInteger(obj.GetInt());
    else if (obj.IsNumber())
        return Property::fromNumber(obj.GetFloat());
    else if (obj.IsArray()) {
        const auto& array = obj.GetArray();
        const size_t len  = array.Size();

        if (len >= 1 && array[0].IsObject())
            return Property::fromTransform(getTransform(array));
        else if (len == 2)
            return Property::fromVector2(getVector2f(obj));
        else if (len == 3)
            return Property::fromVector3(getVector3f(obj));
        else if (len == 9)
            return Property::fromTransform(Transformf(getMatrix3f(obj)));
        else if (len == 12 || len == 16)
            return Property::fromTransform(Transformf(getMatrix4f(obj)));
    } else if (obj.IsObject()) {
        // Deprecated way of handling transforms
        Transformf transform = Transformf::Identity();

        applyTransformProperty(transform, obj.GetObject());
        return Property::fromTransform(transform);
    }

    return Property();
}

inline static void populateObject(std::shared_ptr<Object>& ptr, const rapidjson::Value& obj)
{
    for (auto itr = obj.MemberBegin(); itr != obj.MemberEnd(); ++itr) {
        const std::string name = getString(itr->name);

        // Skip name and type entries
        if (name == "name" || name == "type")
            continue;

        const auto prop = getProperty(itr->value);
        if (prop.isValid())
            ptr->setProperty(name, prop);
    }
}

static std::shared_ptr<Object> handleAnonymousObject(Scene& scene, ObjectType type, const std::filesystem::path& baseDir, const rapidjson::Value& obj)
{
    IG_UNUSED(scene);

    if (obj.HasMember("type") && !obj["type"].IsString())
        throw std::runtime_error("Expected type to be a string");

    const auto pluginType = obj.HasMember("type") ? getString(obj["type"]) : "";

    auto ptr = std::make_shared<Object>(type, pluginType, baseDir);
    populateObject(ptr, obj);
    return ptr;
}

static void handleNamedObject(Scene& scene, ObjectType type, const std::filesystem::path& baseDir, const rapidjson::Value& obj)
{
    if (obj.HasMember("type") && !obj["type"].IsString())
        throw std::runtime_error("Expected type to be a string");

    if (!obj.HasMember("name"))
        throw std::runtime_error("Expected name");

    if (!obj["name"].IsString())
        throw std::runtime_error("Expected name to be a string");

    const auto pluginType = obj.HasMember("type") ? getString(obj["type"]) : "";
    const auto name       = getString(obj["name"]);

    auto ptr = std::make_shared<Object>(type, pluginType, baseDir);
    populateObject(ptr, obj);

    switch (type) {
    case OT_SHAPE:
        scene.addShape(name, ptr);
        break;
    case OT_TEXTURE:
        scene.addTexture(name, ptr);
        break;
    case OT_BSDF:
        scene.addBSDF(name, ptr);
        break;
    case OT_LIGHT:
        scene.addLight(name, ptr);
        break;
    case OT_MEDIUM:
        scene.addMedium(name, ptr);
        break;
    case OT_ENTITY:
        scene.addEntity(name, ptr);
        break;
    default:
        IG_ASSERT(false, "Unknown object type");
        break;
    }
}

static void handleExternalObject(SceneParser& loader, Scene& scene, const std::filesystem::path& baseDir, const rapidjson::Value& obj)
{
    if (obj.HasMember("type") && !obj["type"].IsString())
        throw std::runtime_error("Expected type to be a string");

    if (!obj.HasMember("filename"))
        throw std::runtime_error("Expected a path for externals");

    std::string pluginType           = obj.HasMember("type") ? to_lowercase(getString(obj["type"])) : "";
    const std::string inc_path       = getString(obj["filename"]);
    const std::filesystem::path path = std::filesystem::canonical(resolvePath(inc_path, baseDir));
    if (path.empty())
        throw std::runtime_error("Could not find path '" + inc_path + "'");

    // If type is empty, determine type by file extension
    if (pluginType.empty()) {
        if (to_lowercase(path.extension().u8string()) == ".json")
            pluginType = "ignis";
        else if (to_lowercase(path.extension().u8string()) == ".gltf" || to_lowercase(path.extension().u8string()) == ".glb")
            pluginType = "gltf";
        else
            throw std::runtime_error("Could not determine external type by filename '" + path.u8string() + "'");
    }

    if (pluginType == "ignis") {
        // Include ignis file
        bool ok           = false;
        Scene local_scene = loader.loadFromFile(path, ok);

        if (!ok)
            throw std::runtime_error("Could not load '" + path.generic_u8string() + "'");

        scene.addFrom(local_scene);
    } else if (pluginType == "gltf") {
        // Include and map gltf stuff
        bool ok           = false;
        Scene local_scene = glTFSceneParser::loadFromFile(path, ok);

        if (!ok)
            throw std::runtime_error("Could not load '" + path.generic_u8string() + "'");

        scene.addFrom(local_scene);
    } else {
        throw std::runtime_error("Unknown external type '" + pluginType + "' given");
    }
}

void Scene::addFrom(const Scene& other)
{
    for (const auto& tex : other.textures())
        addTexture(tex.first, tex.second);
    for (const auto& bsdf : other.bsdfs())
        addBSDF(bsdf.first, bsdf.second);
    for (const auto& light : other.lights())
        addLight(light.first, light.second);
    for (const auto& medium : other.media())
        addLight(medium.first, medium.second);
    for (const auto& shape : other.shapes())
        addShape(shape.first, shape.second);
    for (const auto& ent : other.entities())
        addEntity(ent.first, ent.second);

    // Ignore technique, camera & film if already specified
    if (!mTechnique)
        mTechnique = other.mTechnique;

    if (!mCamera)
        mCamera = other.mCamera;

    if (!mFilm)
        mFilm = other.mFilm;
}

void Scene::addConstantEnvLight()
{
    if (mLights.count("__env") == 0) {
        auto env = std::make_shared<Object>(OT_LIGHT, "constant", std::filesystem::path{});
        env->setProperty("radiance", Property::fromNumber(1));
        addLight("__env", env);
    }
}

class InternalSceneParser {
public:
    static Scene loadFromJSON(SceneParser& loader, const std::filesystem::path& baseDir, const rapidjson::Document& doc)
    {
        if (!doc.IsObject())
            throw std::runtime_error("Expected root element to be an object");

        Scene scene;

        if (doc.HasMember("camera"))
            scene.setCamera(handleAnonymousObject(scene, OT_CAMERA, baseDir, doc["camera"]));

        if (doc.HasMember("technique"))
            scene.setTechnique(handleAnonymousObject(scene, OT_TECHNIQUE, baseDir, doc["technique"]));

        if (doc.HasMember("film"))
            scene.setFilm(handleAnonymousObject(scene, OT_FILM, baseDir, doc["film"]));

        if (doc.HasMember("externals")) {
            if (!doc["externals"].IsArray())
                throw std::runtime_error("Expected external elements to be an array");
            for (const auto& exts : doc["externals"].GetArray()) {
                if (!exts.IsObject())
                    throw std::runtime_error("Expected external element to be an object");

                handleExternalObject(loader, scene, baseDir, exts);
            }
        }

        if (doc.HasMember("shapes")) {
            if (!doc["shapes"].IsArray())
                throw std::runtime_error("Expected shapes element to be an array");
            for (const auto& shape : doc["shapes"].GetArray()) {
                if (!shape.IsObject())
                    throw std::runtime_error("Expected shape element to be an object");
                handleNamedObject(scene, OT_SHAPE, baseDir, shape);
            }
        }

        if (doc.HasMember("textures")) {
            if (!doc["textures"].IsArray())
                throw std::runtime_error("Expected textures element to be an array");
            for (const auto& tex : doc["textures"].GetArray()) {
                if (!tex.IsObject())
                    throw std::runtime_error("Expected texture element to be an object");
                handleNamedObject(scene, OT_TEXTURE, baseDir, tex);
            }
        }

        if (doc.HasMember("bsdfs")) {
            if (!doc["bsdfs"].IsArray())
                throw std::runtime_error("Expected bsdfs element to be an array");
            for (const auto& bsdf : doc["bsdfs"].GetArray()) {
                if (!bsdf.IsObject())
                    throw std::runtime_error("Expected bsdf element to be an object");
                handleNamedObject(scene, OT_BSDF, baseDir, bsdf);
            }
        }

        if (doc.HasMember("lights")) {
            if (!doc["lights"].IsArray())
                throw std::runtime_error("Expected lights element to be an array");
            for (const auto& light : doc["lights"].GetArray()) {
                if (!light.IsObject())
                    throw std::runtime_error("Expected light element to be an object");
                handleNamedObject(scene, OT_LIGHT, baseDir, light);
            }
        }

        if (doc.HasMember("media")) {
            if (!doc["media"].IsArray())
                throw std::runtime_error("Expected lights element to be an array");
            for (const auto& medium : doc["media"].GetArray()) {
                if (!medium.IsObject())
                    throw std::runtime_error("Expected medium element to be an object");
                handleNamedObject(scene, OT_MEDIUM, baseDir, medium);
            }
        }

        if (doc.HasMember("entities")) {
            if (!doc["entities"].IsArray())
                throw std::runtime_error("Expected entities element to be an array");
            for (const auto& entity : doc["entities"].GetArray()) {
                if (!entity.IsObject())
                    throw std::runtime_error("Expected entity element to be an object");
                handleNamedObject(scene, OT_ENTITY, baseDir, entity);
            }
        }

        return scene;
    }
};

constexpr auto JsonFlags = rapidjson::kParseDefaultFlags | rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag | rapidjson::kParseNanAndInfFlag | rapidjson::kParseEscapedApostropheFlag;

Scene SceneParser::loadFromFile(const std::filesystem::path& path, bool& ok)
{
    if (path.extension() == ".gltf" || path.extension() == ".glb") {
        // Load gltf directly
        Scene scene = glTFSceneParser::loadFromFile(path, ok);
        if (ok) {
            // Scene is using volumes, switch to the volpath technique
            if (!scene.media().empty())
                scene.setTechnique(std::make_shared<Object>(OT_TECHNIQUE, "volpath", path.parent_path()));

            // Add light to the scene if no light is available (preview style)
            if (scene.lights().empty()) {
                IG_LOG(L_WARNING) << "No lights available in " << path << ". Adding default environment light" << std::endl;
                scene.addConstantEnvLight();
            }
        }
        return scene;
    }

    std::ifstream ifs(path.generic_u8string());
    if (!ifs.good()) {
        ok = false;
        IG_LOG(L_ERROR) << "Could not open file '" << path << "'" << std::endl;
        return Scene();
    }

    rapidjson::IStreamWrapper isw(ifs);

    ok = true;
    rapidjson::Document doc;
    if (doc.ParseStream<JsonFlags>(isw).HasParseError()) {
        ok = false;
        IG_LOG(L_ERROR) << "JSON[" << doc.GetErrorOffset() << "]: " << rapidjson::GetParseError_En(doc.GetParseError()) << std::endl;
        return Scene();
    }

    const std::filesystem::path parent = path.has_parent_path() ? std::filesystem::canonical(path.parent_path()) : std::filesystem::path{};
    return InternalSceneParser::loadFromJSON(*this, parent, doc);
}

Scene SceneParser::loadFromString(const std::string& str, const std::filesystem::path& opt_dir, bool& ok)
{
    rapidjson::Document doc;
    if (doc.Parse<JsonFlags>(str.c_str()).HasParseError()) {
        ok = false;
        IG_LOG(L_ERROR) << "JSON[" << doc.GetErrorOffset() << "]: " << rapidjson::GetParseError_En(doc.GetParseError()) << std::endl;
        return Scene();
    }
    ok = true;
    return InternalSceneParser::loadFromJSON(*this, opt_dir, doc);
}

} // namespace IG::Parser