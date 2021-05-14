#pragma once

#include "LoaderEnvironment.h"
#include "Target.h"

#include <filesystem>

namespace IG {

struct SceneDatabase;
struct LoaderContext {
	Parser::Scene Scene;

	std::filesystem::path FilePath;
	IG::Target Target;
	bool EnablePadding;
	std::unordered_map<std::string, uint32> Images; // Image to Buffer

	LoaderEnvironment Environment;

	inline std::filesystem::path handlePath(const std::filesystem::path& path) const
	{
		if (path.is_absolute())
			return path;
		else {
			const auto p = std::filesystem::canonical(FilePath.parent_path() / path);
			if (std::filesystem::exists(p))
				return p;
			else
				return std::filesystem::canonical(path);
		}
	}

	bool isTexture(const std::shared_ptr<Parser::Object>& obj, const std::string& propname) const;
	uint32 extractTextureID(const std::shared_ptr<Parser::Object>& obj, const std::string& propname) const;
	Vector3f extractColor(const std::shared_ptr<Parser::Object>& obj, const std::string& propname, const Vector3f& def = Vector3f::Ones()) const;

	uint32 loadImage(const std::filesystem::path& path, SceneDatabase& dtb, bool& ok);
};

} // namespace IG