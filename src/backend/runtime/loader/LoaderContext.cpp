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

} // namespace IG