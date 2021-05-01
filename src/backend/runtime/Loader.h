#pragma once

#include "Target.h"
#include "table/SceneDatabase.h"

namespace IG {
struct LoaderOptions {
	IG::Target Target;
	size_t Device;
};

struct LoaderRenderSettings {
	uint32 FilmWidth   = 0;
	uint32 FilmHeight  = 0;
	Vector3f CameraEye = Vector3f::Zero();
	Vector3f CameraDir = Vector3f::UnitZ();
	Vector3f CameraUp  = Vector3f::UnitY();
	float FOV;
};

struct LoaderResult {
	SceneDatabase Database;
	uint64 DesiredConfiguration;
	LoaderRenderSettings DesiredSettings;
};

class Loader {
public:
	static bool load(const std::filesystem::path& filepath, const LoaderOptions& opts, LoaderResult& result);
};
} // namespace IG