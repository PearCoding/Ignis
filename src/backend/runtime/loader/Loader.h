#pragma once

#include "Parser.h"
#include "Target.h"
#include "table/SceneDatabase.h"

namespace IG {
constexpr size_t DefaultAlignment = sizeof(float) * 4;

struct LoaderOptions {
	std::filesystem::path FilePath;
	Parser::Scene Scene;
	IG::Target Target;
	std::string CameraType;
	std::string TechniqueType;
	size_t SamplesPerIteration;
};

struct LoaderResult {
	SceneDatabase Database;
	size_t AOVCount;
	std::string RayGenerationShader;
	std::string MissShader;
	std::vector<std::string> HitShaders;
};

class Loader {
public:
	static bool load(const LoaderOptions& opts, LoaderResult& result);
};
} // namespace IG