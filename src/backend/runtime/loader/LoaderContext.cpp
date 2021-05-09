#include "LoaderContext.h"

#include "Logger.h"

namespace IG {

using namespace Parser;

bool LoaderContext::isTexture(const std::shared_ptr<Parser::Object>& obj, const std::string& propname) const
{
	auto prop = obj->property(propname);
	return prop.isValid() && prop.type() == PT_STRING;
}

uint32 LoaderContext::extractTextureID(const std::shared_ptr<Parser::Object>& obj, const std::string& propname) const
{
	return 0; // TODO
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
		default:
			IG_LOG(L_WARNING) << "Unknown property type for '" << propname << "'" << std::endl;
			return def;
		}
	} else {
		return def;
	}
}

std::string LoaderContext::extractMaterialPropertyColorLight(const std::shared_ptr<Parser::Object>& obj, const std::string& propname, float def, bool& isTexture) const
{
	isTexture = false;
	std::stringstream sstream;

	auto prop = obj->property(propname);
	if (prop.isValid()) {
		switch (prop.type()) {
		case PT_INTEGER:
			sstream << "make_gray_color(" << prop.getInteger() << ")";
			break;
		case PT_NUMBER:
			sstream << "make_gray_color(" << prop.getNumber() << ")";
			break;
		case PT_VECTOR3: {
			auto v_rgb = prop.getVector3();
			sstream << "make_color(" << v_rgb(0) << ", " << v_rgb(1) << ", " << v_rgb(2) << ")";
		} break;
		case PT_STRING:
			sstream << "tex_" << prop.getString();
			isTexture = true;
			break;
		default:
			IG_LOG(L_WARNING) << "Unknown property type for '" << propname << "'" << std::endl;
			sstream << "black";
			break;
		}
	} else {
		sstream << "make_gray_color(" << def << ")";
	}

	return sstream.str();
}

std::string LoaderContext::extractMaterialPropertyColor(const std::shared_ptr<Parser::Object>& obj, const std::string& propname, float def, const char* surfParameter) const
{
	std::stringstream sstream;

	auto prop = obj->property(propname);
	if (prop.isValid()) {
		switch (prop.type()) {
		case PT_INTEGER:
			sstream << "make_gray_color(" << prop.getInteger() << ")";
			break;
		case PT_NUMBER:
			sstream << "make_gray_color(" << prop.getNumber() << ")";
			break;
		case PT_VECTOR3: {
			auto v_rgb = prop.getVector3();
			sstream << "make_color(" << v_rgb(0) << ", " << v_rgb(1) << ", " << v_rgb(2) << ")";
		} break;
		case PT_STRING:
			sstream << "tex_" << prop.getString() << "(vec4_to_2(" << surfParameter << ".attr(0)))";
			break;
		default:
			IG_LOG(L_WARNING) << "Unknown property type for '" << propname << "'" << std::endl;
			sstream << "black";
			break;
		}
	} else {
		sstream << "make_gray_color(" << def << ")";
	}

	return sstream.str();
}

std::string LoaderContext::extractMaterialPropertyNumber(const std::shared_ptr<Parser::Object>& obj, const std::string& propname, float def, const char* surfParameter) const
{
	std::stringstream sstream;

	auto prop = obj->property(propname);
	if (prop.isValid()) {
		switch (prop.type()) {
		case PT_INTEGER:
			sstream << prop.getInteger();
			break;
		case PT_NUMBER:
			sstream << prop.getNumber();
			break;
		case PT_VECTOR3: {
			auto v_rgb = prop.getVector3();
			if (v_rgb(0) == v_rgb(1) && v_rgb(0) == v_rgb(2)) { // Same value everywhere
				sstream << v_rgb(0);
			} else {
				IG_LOG(L_WARNING) << "RGB input for '" << propname << "' not supported. Using average instead" << std::endl;
				sstream << v_rgb.mean();
			}
		} break;
		case PT_STRING:
			sstream << "tex_" << prop.getString() << "(vec4_to_2(" << surfParameter << ".attr(0))).r";
			break;
		default:
			IG_LOG(L_WARNING) << "Unknown property type for '" << propname << "'" << std::endl;
			sstream << "black";
			break;
		}
	} else {
		sstream << def;
	}

	return sstream.str();
}

std::string LoaderContext::extractMaterialPropertyNumberDx(const std::shared_ptr<Parser::Object>& obj, const std::string& propname, const char* surfParameter) const
{
	std::stringstream sstream;

	auto prop = obj->property(propname);
	if (prop.isValid()) {
		switch (prop.type()) {
		case PT_INTEGER:
		case PT_NUMBER:
		case PT_VECTOR3:
			sstream << 0.0f;
			break;
		case PT_STRING:
			sstream << "texture_dx(tex_" << prop.getString() << ", vec4_to_2(" << surfParameter << ".attr(0))).r";
			break;
		default:
			IG_LOG(L_WARNING) << "Unknown property type for '" << propname << "'" << std::endl;
			sstream << 0.0f;
			break;
		}
	} else {
		sstream << 0.0f;
	}

	return sstream.str();
}

std::string LoaderContext::extractMaterialPropertyNumberDy(const std::shared_ptr<Parser::Object>& obj, const std::string& propname, const char* surfParameter) const
{
	std::stringstream sstream;

	auto prop = obj->property(propname);
	if (prop.isValid()) {
		switch (prop.type()) {
		case PT_INTEGER:
		case PT_NUMBER:
		case PT_VECTOR3:
			sstream << 0.0f;
			break;
		case PT_STRING:
			sstream << "texture_dy(tex_" << prop.getString() << ", vec4_to_2(" << surfParameter << ".attr(0))).r";
			break;
		default:
			IG_LOG(L_WARNING) << "Unknown property type for '" << propname << "'" << std::endl;
			sstream << 0.0f;
			break;
		}
	} else {
		sstream << 0.0f;
	}

	return sstream.str();
}

} // namespace IG