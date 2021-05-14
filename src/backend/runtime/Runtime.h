#pragma once

#include "DebugMode.h"
#include "driver/DriverManager.h"
#include "loader/Loader.h"
#include "table/SceneDatabase.h"

namespace IG {
class Camera;
struct LoaderOptions;

struct RuntimeOptions {
	Target DesiredTarget;
	uint32 Device;
	std::string OverrideTechnique;
	std::string OverrideCamera;
};

struct RuntimeRenderSettings {
	uint32 FilmWidth	 = 800;
	uint32 FilmHeight	 = 600;
	Vector3f CameraEye	 = Vector3f::Zero();
	Vector3f CameraDir	 = Vector3f::UnitZ();
	Vector3f CameraUp	 = Vector3f::UnitY();
	float FOV			 = 60;
	float TMin			 = 0;
	float TMax			 = std::numeric_limits<float>::max();
	uint32 MaxPathLength = 64;
};

struct Ray {
	Vector3f Origin;
	Vector3f Direction;
	Vector2f Range;
};

class Runtime {
public:
	Runtime(const std::filesystem::path& path, const RuntimeOptions& opts);
	~Runtime();

	void setup(uint32 framebuffer_width, uint32 framebuffer_height);
	void step(const Camera& camera);
	void trace(const std::vector<Ray>& rays, std::vector<float>& data);

	const float* getFramebuffer(int aov = 0) const;
	// aov<0 will clear all aovs
	void clearFramebuffer(int aov = -1);
	inline uint32 iterationCount() const { return mIteration; }

	inline const RuntimeRenderSettings& loadedRenderSettings() const { return mLoadedRenderSettings; }

	inline DebugMode currentDebugMode() const { return mDebugMode; }
	inline void setDebugMode(DebugMode mode) { mDebugMode = mode; }

	inline uint32 configuration() const { return mConfiguration; }
private:
	void shutdown();

	bool mInit;

	SceneDatabase mDatabase;
	RuntimeRenderSettings mLoadedRenderSettings;
	DriverInterface mLoadedInterface;
	DriverManager mManager;

	size_t mDevice;
	uint32 mIteration;
	uint32 mConfiguration;
	DebugMode mDebugMode;
};
} // namespace IG