#include "Parser.h"
#include "Logger.h"

#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/istreamwrapper.h>

#include "glTFParser.h"

namespace IG {
namespace Parser {
// TODO: (Re)Add arguments!

using LookupPaths = std::vector<std::string>;
using Arguments   = std::unordered_map<std::string, std::string>;

// ------------- File IO
static inline bool doesFileExist(const std::string& fileName)
{
    return std::ifstream(fileName).good();
}

static inline std::string concactPaths(const std::string& a, const std::string& b)
{
    if (a.empty())
        return b;
    else if (b.empty())
        return a;

    if (a.back() != '/' && a.back() != '\\' && a.back() != b.front())
        return a + '/' + b;
    else
        return a + b;
}

static inline std::string extractDirectoryOfPath(const std::string& str)
{
    size_t found = str.find_last_of("/\\");
    return (found == std::string::npos) ? "" : str.substr(0, found);
}

static inline std::string resolvePath(const std::string& path, const LookupPaths& lookups)
{
    for (const auto& dir : lookups) {
        const std::string p = concactPaths(dir, path);
        if (doesFileExist(p))
            return p;
    }

    if (doesFileExist(path))
        return path;
    else
        return "";
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

inline static Vector3f getVector3f(const rapidjson::Value& obj)
{
    const auto& array = obj.GetArray();
    const size_t len  = array.Size();
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
            mat(i, j) = array[i * 3 + j].GetFloat();
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
            mat(i, j) = array[i * 4 + j].GetFloat();
    return mat;
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

        if (len == 2)
            return Property::fromVector2(getVector2f(obj));
        else if (len == 3)
            return Property::fromVector3(getVector3f(obj));
        else if (len == 9)
            return Property::fromTransform(Transformf(getMatrix3f(obj)));
        else if (len == 16)
            return Property::fromTransform(Transformf(getMatrix4f(obj)));
    }

    return Property();
}

template <typename Derived1, typename Derived2, typename Derived3>
Eigen::Matrix<typename Derived1::Scalar, 3, 4> lookAt(const Eigen::MatrixBase<Derived1>& eye,
                                                      const Eigen::MatrixBase<Derived2>& center,
                                                      const Eigen::MatrixBase<Derived3>& up)
{
    typedef Eigen::Matrix<typename Derived1::Scalar, 3, 1> Vector3;
    typedef Eigen::Matrix<typename Derived1::Scalar, 3, 4> Matrix34;

    Vector3 f = (center - eye).normalized();
    Vector3 u = up.normalized();
    Vector3 s = f.cross(u).normalized();
    u         = s.cross(f);

    Matrix34 m;
    m.col(0) = s;
    m.col(1) = u;
    m.col(2) = f;
    m.col(3) = eye;

    return m;
}

inline static void populateObject(std::shared_ptr<Object>& ptr, const rapidjson::Value& obj)
{
    for (auto itr = obj.MemberBegin(); itr != obj.MemberEnd(); ++itr) {
        const std::string name = getString(itr->name);

        // Skip name and type entries
        if (name == "name" || name == "type")
            continue;

        if (name == "transform") {
            if (itr->value.IsObject()) {
                Transformf transform = Transformf::Identity();

                // From left to right. The last entry will be applied first to a potential point A1*A2*A3*...*An*p
                for (auto val = itr->value.MemberBegin(); val != itr->value.MemberEnd(); ++val) {
                    if (val->name == "translate") {
                        const Vector3f pos = getVector3f(val->value);
                        transform.translate(pos);
                    } else if (val->name == "scale") {
                        if (val->value.IsNumber()) {
                            transform.scale(val->value.GetFloat());
                        } else {
                            const Vector3f s = getVector3f(val->value);
                            transform.scale(s);
                        }
                    } else if (val->name == "rotate") { // [Rotation around X, Rotation around Y, Rotation around Z] all in degrees
                        const Vector3f angles = getVector3f(val->value);
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
                ptr->setProperty(name, Property::fromTransform(transform));
            } else if (itr->value.IsArray()) {
                const size_t len = itr->value.GetArray().Size();
                if (len == 0)
                    ptr->setProperty(name, Property::fromTransform(Transformf::Identity()));
                else if (len == 9)
                    ptr->setProperty(name, Property::fromTransform(Transformf(getMatrix3f(itr->value))));
                else if (len == 12 || len == 16)
                    ptr->setProperty(name, Property::fromTransform(Transformf(getMatrix4f(itr->value))));
                else
                    throw std::runtime_error("Expected transform property to be an array of size 9 or 16");
            } else {
                throw std::runtime_error("Expected transform property to be an object");
            }
        } else {
            const auto prop = getProperty(itr->value);
            if (prop.isValid())
                ptr->setProperty(name, prop);
        }
    }
}

static std::shared_ptr<Object> handleAnonymousObject(Scene& scene, ObjectType type, const rapidjson::Value& obj)
{
    IG_UNUSED(scene);

    if (obj.HasMember("type")) {
        if (!obj["type"].IsString())
            throw std::runtime_error("Expected type to be a string");
    }

    const auto pluginType = obj.HasMember("type") ? getString(obj["type"]) : "";

    auto ptr = std::make_shared<Object>(type, pluginType);
    populateObject(ptr, obj);
    return ptr;
}

static void handleNamedObject(Scene& scene, ObjectType type, const rapidjson::Value& obj)
{
    if (obj.HasMember("type")) {
        if (!obj["type"].IsString())
            throw std::runtime_error("Expected type to be a string");
    }

    if (!obj.HasMember("name"))
        throw std::runtime_error("Expected name");

    if (!obj["name"].IsString())
        throw std::runtime_error("Expected name to be a string");

    const auto pluginType = obj.HasMember("type") ? getString(obj["type"]) : "";
    const auto name       = getString(obj["name"]);

    auto ptr = std::make_shared<Object>(type, pluginType);
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
    case OT_ENTITY:
        scene.addEntity(name, ptr);
        break;
    default:
        IG_ASSERT(false, "Unknown object type");
        break;
    }
}

static void handleExternalObject(SceneParser& loader, Scene& scene, const rapidjson::Value& obj)
{
    if (obj.HasMember("type")) {
        if (!obj["type"].IsString())
            throw std::runtime_error("Expected type to be a string");
    }

    if (!obj.HasMember("filename"))
        throw std::runtime_error("Expected a path for externals");

    std::string pluginType = obj.HasMember("type") ? getString(obj["type"]) : "ignis";
    std::transform(pluginType.begin(), pluginType.end(), pluginType.begin(), ::tolower);

    const std::string inc_path = getString(obj["filename"]);
    const std::string path     = resolvePath(inc_path, loader.lookupPaths());
    if (path.empty())
        throw std::runtime_error("Could not find path '" + inc_path + "'");

    if (pluginType == "ignis") {
        // Include ignis file but ignore technique, camera & film
        bool ok           = false;
        Scene local_scene = loader.loadFromFile(path, ok);

        if (!ok)
            throw std::runtime_error("Could not load '" + path + "'");

        scene.addFrom(local_scene);
    } else if (pluginType == "gltf") {
        // Include and map gltf stuff
        bool ok           = false;
        Scene local_scene = glTFSceneParser::loadFromFile(path, ok);

        if (!ok)
            throw std::runtime_error("Could not load '" + path + "'");

        scene.addFrom(local_scene);
    } else {
        throw std::runtime_error("Unknown external type '" + pluginType + "' given");
    }
}

void Scene::addFrom(const Scene& other)
{
    for (const auto tex : other.textures())
        addTexture(tex.first, tex.second);
    for (const auto bsdf : other.bsdfs())
        addBSDF(bsdf.first, bsdf.second);
    for (const auto light : other.lights())
        addLight(light.first, light.second);
    for (const auto shape : other.shapes())
        addShape(shape.first, shape.second);
    for (const auto ent : other.entities())
        addEntity(ent.first, ent.second);

    if (!mTechnique) {
        mTechnique = other.mTechnique;
    }

    if (!mCamera) {
        mCamera = other.mCamera;
    }

    if (!mFilm) {
        mFilm = other.mFilm;
    }
}

class InternalSceneParser {
public:
    static Scene loadFromJSON(SceneParser& loader, const rapidjson::Document& doc)
    {
        if (!doc.IsObject())
            throw std::runtime_error("Expected root element to be an object");

        Scene scene;

        if (doc.HasMember("camera")) {
            scene.setCamera(handleAnonymousObject(scene, OT_CAMERA, doc["camera"]));
        }

        if (doc.HasMember("technique")) {
            scene.setTechnique(handleAnonymousObject(scene, OT_TECHNIQUE, doc["technique"]));
        }

        if (doc.HasMember("film")) {
            scene.setFilm(handleAnonymousObject(scene, OT_FILM, doc["film"]));
        }

        if (doc.HasMember("externals")) {
            if (!doc["externals"].IsArray())
                throw std::runtime_error("Expected external elements to be an array");
            for (const auto& exts : doc["externals"].GetArray()) {
                if (!exts.IsObject())
                    throw std::runtime_error("Expected external element to be an object");

                handleExternalObject(loader, scene, exts);
            }
        }

        if (doc.HasMember("shapes")) {
            if (!doc["shapes"].IsArray())
                throw std::runtime_error("Expected shapes element to be an array");
            for (const auto& shape : doc["shapes"].GetArray()) {
                if (!shape.IsObject())
                    throw std::runtime_error("Expected shape element to be an object");
                handleNamedObject(scene, OT_SHAPE, shape);
            }
        }

        if (doc.HasMember("textures")) {
            if (!doc["textures"].IsArray())
                throw std::runtime_error("Expected textures element to be an array");
            for (const auto& tex : doc["textures"].GetArray()) {
                if (!tex.IsObject())
                    throw std::runtime_error("Expected texture element to be an object");
                handleNamedObject(scene, OT_TEXTURE, tex);
            }
        }

        if (doc.HasMember("bsdfs")) {
            if (!doc["bsdfs"].IsArray())
                throw std::runtime_error("Expected bsdfs element to be an array");
            for (const auto& bsdf : doc["bsdfs"].GetArray()) {
                if (!bsdf.IsObject())
                    throw std::runtime_error("Expected bsdf element to be an object");
                handleNamedObject(scene, OT_BSDF, bsdf);
            }
        }

        if (doc.HasMember("lights")) {
            if (!doc["lights"].IsArray())
                throw std::runtime_error("Expected lights element to be an array");
            for (const auto& light : doc["lights"].GetArray()) {
                if (!light.IsObject())
                    throw std::runtime_error("Expected light element to be an object");
                handleNamedObject(scene, OT_LIGHT, light);
            }
        }

        if (doc.HasMember("entities")) {
            if (!doc["entities"].IsArray())
                throw std::runtime_error("Expected entities element to be an array");
            for (const auto& entity : doc["entities"].GetArray()) {
                if (!entity.IsObject())
                    throw std::runtime_error("Expected entity element to be an object");
                handleNamedObject(scene, OT_ENTITY, entity);
            }
        }

        return scene;
    }
};

constexpr auto JsonFlags = rapidjson::kParseDefaultFlags | rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag | rapidjson::kParseNanAndInfFlag | rapidjson::kParseEscapedApostropheFlag;

Scene SceneParser::loadFromFile(const char* path, bool& ok)
{
    if (std::filesystem::path(path).extension() == ".gltf" || std::filesystem::path(path).extension() == ".glb") {
        // Load gltf directly
        Scene scene = glTFSceneParser::loadFromFile(path, ok);
        if (ok) {
            // Add a constant env light to see at least something
            if (scene.lights().empty()) {
                auto env = std::make_shared<Object>(OT_LIGHT, "sky");
                scene.addLight("__env", env);
            }
        }
        return scene;
    }

    std::ifstream ifs(path);
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

    const auto dir = extractDirectoryOfPath(path);
    if (dir.empty()) {
        return InternalSceneParser::loadFromJSON(*this, doc);
    } else {
        mLookupPaths.insert(mLookupPaths.begin(), dir);
        const auto res = InternalSceneParser::loadFromJSON(*this, doc);
        mLookupPaths.erase(mLookupPaths.begin());
        return res;
    }
}

Scene SceneParser::loadFromString(const char* str, bool& ok)
{
    rapidjson::Document doc;
    if (doc.Parse<JsonFlags>(str).HasParseError()) {
        ok = false;
        IG_LOG(L_ERROR) << "JSON[" << doc.GetErrorOffset() << "]: " << rapidjson::GetParseError_En(doc.GetParseError()) << std::endl;
        return Scene();
    }
    ok = true;
    return InternalSceneParser::loadFromJSON(*this, doc);
}

Scene SceneParser::loadFromString(const char* str, size_t max_len, bool& ok)
{
    rapidjson::Document doc;
    if (doc.Parse<JsonFlags>(str, max_len).HasParseError()) {
        ok = false;
        IG_LOG(L_ERROR) << "JSON[" << doc.GetErrorOffset() << "]: " << rapidjson::GetParseError_En(doc.GetParseError()) << std::endl;
        return Scene();
    }
    ok = true;
    return InternalSceneParser::loadFromJSON(*this, doc);
}
} // namespace Parser
} // namespace IG