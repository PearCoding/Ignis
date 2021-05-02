#include "Runtime.h"
#include "Camera.h"
#include "Loader.h"

namespace IG {
Runtime::Runtime(const std::filesystem::path& path, const LoaderOptions& opts)
	: mInit(false)
	, mDevice(opts.Device)
	, mIteration(0)
{
	if (!mManager.init())
		throw std::runtime_error("Could not init modules!");

	// TODO: Check target!
	LoaderOptions m_opts = opts;

	LoaderResult result;
	if (!Loader::load(path, m_opts, result))
		throw std::runtime_error("Could not load scene!");
	mLoadedRenderSettings = result.DesiredSettings;

	const uint64 newConfig = mManager.checkConfiguration(result.DesiredConfiguration);
	if (newConfig != result.DesiredConfiguration) {
		// TODO: Inform user about config change!
	}

	if (!mManager.load(newConfig, mLoadedInterface))
		throw std::runtime_error("Error loading interface!");
}

Runtime::~Runtime()
{
	if (mInit)
		shutdown();
}

void Runtime::step(const Camera& camera)
{
	if (!mInit)
		return;

	DriverRenderSettings settings;
	for (int i = 0; i < 3; ++i)
		settings.eye[i] = camera.eye(i);
	for (int i = 0; i < 3; ++i)
		settings.dir[i] = camera.dir(i);
	for (int i = 0; i < 3; ++i)
		settings.up[i] = camera.up(i);
	for (int i = 0; i < 3; ++i)
		settings.right[i] = camera.right(i);
	settings.width	   = camera.w;
	settings.height	   = camera.h;
	settings.ray_count = 0;
	settings.rays	   = nullptr; // No artifical ray streams
	settings.device	   = mDevice;

	mLoadedInterface.RenderFunction(&settings, mIteration++);
}

const float* Runtime::getFramebuffer(int aov) const
{
	return mLoadedInterface.GetFramebufferFunction(aov);
}

void Runtime::clearFramebuffer(int aov)
{
	return mLoadedInterface.ClearFramebufferFunction(aov);
}

void Runtime::setup(uint32 framebuffer_width, uint32 framebuffer_height)
{
	DriverSetupSettings settings;
	settings.database			= &mDatabase;
	settings.framebuffer_width	= std::max(1u, framebuffer_width);
	settings.framebuffer_height = std::max(1u, framebuffer_height);
	mLoadedInterface.SetupFunction(&settings);
	mInit = true;

	clearFramebuffer();
}

void Runtime::shutdown()
{
	mLoadedInterface.ShutdownFunction();
}

} // namespace IG