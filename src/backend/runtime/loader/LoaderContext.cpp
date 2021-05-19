#include "LoaderContext.h"
#include "Image.h"
#include "Logger.h"
#include "serialization/VectorSerializer.h"
#include "table/SceneDatabase.h"

namespace IG {

using namespace Parser;

bool LoaderContext::isTexture(const std::shared_ptr<Parser::Object>& obj, const std::string& propname) const
{
	auto prop = obj->property(propname);
	return prop.isValid() && prop.type() == PT_STRING;
}

uint32 LoaderContext::extractTextureID(const std::shared_ptr<Parser::Object>& obj, const std::string& propname) const
{
	if (isTexture(obj, propname)) {
		std::string name = obj->property(propname).getString();
		if (Environment.TextureID.count(name)) {
			return Environment.TextureID.at(name);
		} else {
			IG_LOG(L_ERROR) << "No texture '" << name << "' found!" << std::endl;
			return std::numeric_limits<uint32>::max();
		}
	} else {
		IG_LOG(L_ERROR) << "Property '" << propname << "' is not a texture!" << std::endl;
		return std::numeric_limits<uint32>::max();
	}
}

Vector3f LoaderContext::extractColor(const std::shared_ptr<Parser::Object>& obj, const std::string& propname, const Vector3f& def) const
{
	auto prop = obj->property(propname);
	if (prop.isValid()) {
		switch (prop.type()) {
		case PT_INTEGER:
			return Vector3f::Ones() * prop.getInteger();
		case PT_NUMBER:
			return Vector3f::Ones() * prop.getNumber();
		case PT_VECTOR3:
			return prop.getVector3();
		case PT_STRING: {
			IG_LOG(L_WARNING) << "[TODO] Replacing texture by average color" << std::endl;
			std::string name = obj->property(propname).getString();
			if (TextureBuffer.count(name)) {
				uint32 id = TextureBuffer.at(name);
				return TextureAverages.at(id);
			} else {
				IG_LOG(L_ERROR) << "No texture '" << name << "' found!" << std::endl;
				return def;
			}
		}
		default:
			IG_LOG(L_WARNING) << "Unknown property type for '" << propname << "'" << std::endl;
			return def;
		}
	} else {
		return def;
	}
}

float LoaderContext::extractIOR(const std::shared_ptr<Parser::Object>& obj, const std::string& propname, float def) const
{
	auto prop = obj->property(propname);
	if (prop.isValid()) {
		switch (prop.type()) {
		case PT_INTEGER:
			return prop.getInteger();
		case PT_NUMBER:
			return prop.getNumber();
		case PT_VECTOR3:
			return prop.getVector3().mean();
		case PT_STRING: {
			IG_LOG(L_WARNING) << "[TODO] Replacing texture by average color" << std::endl;
			std::string name = obj->property(propname).getString();
			if (TextureBuffer.count(name)) {
				uint32 id = TextureBuffer.at(name);
				return TextureAverages.at(id).mean();
			} else {
				IG_LOG(L_ERROR) << "No texture '" << name << "' found!" << std::endl;
				return def;
			}
		}
		default:
			IG_LOG(L_WARNING) << "Unknown property type for '" << propname << "'" << std::endl;
			return def;
		}
	} else {
		return def;
	}
}

uint32 LoaderContext::loadImage(const std::filesystem::path& path, SceneDatabase& dtb, bool& ok)
{
	const auto real_path		= handlePath(path);
	const std::string hash_able = real_path.generic_u8string();

	if (Images.count(hash_able) == 0) {
		auto image = ImageRgba32::load(real_path);

		if (!image.isValid()) {
			IG_LOG(L_ERROR) << "Could not load image " << real_path << std::endl;
			ok = false;
			return 0;
		}

		// Calculate average as needed to replace textures currently
		Vector3f avg = Vector3f::Zero();
		float inv_s	 = 1.0f / (image.width * image.height);
		for (size_t i = 0; i < image.width * image.height; i += 4) {
			avg += Vector3f(
					   image.pixels[i * 4 + 0],
					   image.pixels[i * 4 + 1],
					   image.pixels[i * 4 + 2])
				   * inv_s;
		}
		TextureAverages.push_back(avg);

		// Register via buffer id
		uint32 id	  = dtb.BufferTable.entryCount();
		uint32 typeID = 0; // TODO: In future we might use this type specifier
		auto& data	  = dtb.BufferTable.addLookup(typeID, 4 * sizeof(float));

		data.reserve(data.size() + sizeof(float) * 4 * image.width * image.height + sizeof(uint32) * 4);

		VectorSerializer serializer(data, false);
		serializer.write((uint32)image.width);
		serializer.write((uint32)image.height);
		serializer.write((uint32)0); // Padding
		serializer.write((uint32)0); // Padding
		serializer.writeRaw(reinterpret_cast<uint8*>(image.pixels.get()), sizeof(float) * 4 * image.width * image.height);

		Images[hash_able] = id;
		ok				  = true;
		return id;
	} else {
		ok = true;
		return Images.at(hash_able);
	}
}

} // namespace IG