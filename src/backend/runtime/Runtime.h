#pragma once

#include "driver/DriverManager.h"
#include "table/SceneDatabase.h"

namespace IG {
class Camera;
struct LoaderOptions;
class Runtime {
public:
	Runtime(const std::filesystem::path& path, const LoaderOptions& opts);
	~Runtime();

	void setup(uint32 framebuffer_width, uint32 framebuffer_height);
	void step(const Camera& camera);

	const float* getFramebuffer(int aov = 0) const;
	// aov<0 will clear all aovs
	void clearFramebuffer(int aov = -1);
	inline uint32 iterationCount() const { return mIteration; }

private:
	void shutdown();

	bool mInit;

	SceneDatabase mDatabase;
	DriverInterface mLoadedInterface;
	DriverManager mManager;

	size_t mDevice;
	uint32 mIteration;
};
} // namespace IG