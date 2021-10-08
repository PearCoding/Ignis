#pragma once

#include "LoaderContext.h"
#include <unordered_map>

namespace IG {
namespace Parser {
class Object;
class Property;
} // namespace Parser

class ShadingTree {
public:
	ShadingTree(const std::string& prefix = "");

	void addNumber(const std::string& name, const LoaderContext& ctx, const Parser::Object& obj, float def = 0);
	void addColor(const std::string& name, const LoaderContext& ctx, const Parser::Object& obj, const Vector3f& def = Vector3f::Zero());

	std::string pullHeader();
	std::string getInline(const std::string& name) const;

private:
	std::string lookupTexture(const std::string& name, const LoaderContext& ctx);

	const std::string mPrefix;
	std::vector<std::string> mHeaderLines;// The order matters
	std::unordered_set<std::string> mLoadedTextures;
	std::unordered_map<std::string, std::string> mParameters;
};
} // namespace IG