#pragma once

#include "GeneratorEnvironment.h"
#include "Target.h"

namespace IG {

struct GeneratorContext {
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

	inline std::string makeId(const std::filesystem::path& path) const
	{
		std::string id = path; // TODO?
		std::transform(id.begin(), id.end(), id.begin(), [](char c) {
			if (std::isspace(c) || !std::isalnum(c))
				return '_';
			return c;
		});
		return id;
	}

	std::string generateTextureLoadInstruction(const std::string& filename, std::string* tex_hndl = nullptr) const;

	void registerTexturesFromBSDF(const std::shared_ptr<TPMObject>& obj);
	void registerTexturesFromLight(const std::shared_ptr<TPMObject>& obj);

	std::string extractMaterialPropertyColor(const std::shared_ptr<TPMObject>& obj, const std::string& propname, float def = 0.0f) const;
	std::string extractMaterialPropertyNumber(const std::shared_ptr<TPMObject>& obj, const std::string& propname, float def = 0.0f) const;
	std::string extractTexture(const std::shared_ptr<TPMObject>& tex) const;
};

}