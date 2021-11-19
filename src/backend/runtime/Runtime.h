#pragma once

#include "DebugMode.h"
#include "Statistics.h"
#include "driver/DriverManager.h"
#include "loader/Loader.h"
#include "table/SceneDatabase.h"

namespace IG {
class Camera;
struct LoaderOptions;

struct RuntimeOptions {
	bool DumpShader		 = false;
	bool AcquireStats	 = false;
	Target DesiredTarget = Target::INVALID;
	bool RecommendCPU	 = true;
	bool RecommendGPU	 = true;
	uint32 Device		 = 0;
	uint32 SPI			 = 0; // Detect automatically
	std::string OverrideTechnique;
	std::string OverrideCamera;
};

struct RuntimeRenderSettings {
	uint32 FilmWidth   = 800;
	uint32 FilmHeight  = 600;
	Vector3f CameraEye = Vector3f::Zero();
	Vector3f CameraDir = Vector3f::UnitZ();
	Vector3f CameraUp  = Vector3f::UnitY();
	float FOV		   = 60;
	float TMin		   = 0;
	float TMax		   = FltMax;
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

	const Statistics* getStatistics() const;

	inline const RuntimeRenderSettings& loadedRenderSettings() const { return mLoadedRenderSettings; }

	inline DebugMode currentDebugMode() const { return mDebugMode; }
	inline void setDebugMode(DebugMode mode) { mDebugMode = mode; }
	inline bool isDebug() const { return mIsDebug; }
	inline bool isTrace() const { return mIsTrace; }

	inline Target target() const { return mTarget; }
	inline size_t samplesPerIteration() const { return mSamplesPerIteration; }

private:
	void shutdown();

	bool mInit;

	SceneDatabase mDatabase;
	RuntimeRenderSettings mLoadedRenderSettings;
	DriverInterface mLoadedInterface;
	DriverManager mManager;

	size_t mDevice;
	size_t mSamplesPerIteration;
	size_t mRayStateComponentCount;
	uint32 mIteration;
	Target mTarget;

	bool mIsTrace;
	bool mIsDebug;
	DebugMode mDebugMode;
	bool mAcquireStats;

	std::string RayGenerationShader;
	std::string MissShader;
	std::vector<std::string> HitShaders;
};
} // namespace IG