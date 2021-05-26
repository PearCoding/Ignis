#include "Parser.h"
#include "Logger.h"

#include <cmath>
#include <fstream>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/istreamwrapper.h>

namespace IG {
namespace Parser {
// TODO: (Re)Add arguments!

using LookupPaths = std::vector<std::string>;
using Arguments	  = std::unordered_map<std::string, std::string>;

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

inline static Vector2f getVector2f(const rapidjson::Value& obj)
{
	const auto& array = obj.GetArray();
	const size_t len  = array.Size();
	if (len != 2)
		throw std::runtime_error("Expected vector of length 2");
	return Vector2f(array[0].GetFloat(), array[1].GetFloat());
}

inline static Vector3f getVector3f(const rapidjson::Value& obj)
{
	const auto& array = obj.GetArray();
	const size_t len  = array.Size();
	if (len != 3)
		throw std::runtime_error("Expected vector of length 3");
	return Vector3f(array[0].GetFloat(), array[1].GetFloat(), array[2].GetFloat());
}

inline static Matrix3f getMatrix3f(const rapidjson::Value& obj)
{
	const auto& array = obj.GetArray();
	const size_t len  = array.Size();
	if (len != 9)
		throw std::runtime_error("Expected matrix of length 9 (3x3)");

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
				for (auto val = itr->value.MemberBegin(); val != itr->value.MemberEnd(); ++val) {
					if (val->name == "position") {
						Vector3f pos = getVector3f(val->value);
						transform.translate(pos);
					} else if (val->name == "scale") {
						if (val->value.IsNumber()) {
							transform.scale(val->value.GetFloat());
						} else {
							Vector3f s = getVector3f(val->value);
							transform.scale(s);
						}
					} else if (val->name == "rotation") {
						// TODO
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
	const auto name		  = getString(obj["name"]);

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

class InternalSceneParser {
public:
	static Scene loadFromJSON(const SceneParser& loader, const rapidjson::Document& doc)
	{
		IG_UNUSED(loader);

		if (!doc.IsObject())
			throw std::runtime_error("Expected root element to be an object");

		Scene scene;

		if (doc.HasMember("camera")) {
			scene.setCamera(handleAnonymousObject(scene, OT_CAMERA, doc["camera"]));
		} else {
			throw std::runtime_error("Expected scene to have a camera");
		}

		if (doc.HasMember("technique")) {
			scene.setTechnique(handleAnonymousObject(scene, OT_TECHNIQUE, doc["technique"]));
		}

		if (doc.HasMember("film")) {
			scene.setFilm(handleAnonymousObject(scene, OT_FILM, doc["film"]));
		} else {
			throw std::runtime_error("Expected scene to have a film");
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