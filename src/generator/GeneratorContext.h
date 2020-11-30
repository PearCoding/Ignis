#pragma once

#include "GeneratorEnvironment.h"
#include "Target.h"

#include <filesystem>

namespace IG {

struct GeneratorContext {
	Loader::Scene Scene;
	
	std::filesystem::path FilePath;
	IG::Target Target;
	size_t MaxPathLen;
	size_t SPP;
	bool Fusion;
	bool EnablePadding;

	GeneratorEnvironment Environment;

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

	std::string extractMaterialPropertyColor(const std::shared_ptr<Loader::Object>& obj, const std::string& propname, float def = 0.0f) const;
	std::string extractMaterialPropertyNumber(const std::shared_ptr<Loader::Object>& obj, const std::string& propname, float def = 0.0f) const;
};

}