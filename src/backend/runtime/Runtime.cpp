#include "Runtime.h"
#include "Camera.h"
#include "Logger.h"
#include "driver/Configuration.h"
#include "loader/Parser.h"

namespace IG {
Runtime::Runtime(const std::filesystem::path& path, const RuntimeOptions& opts)
	: mInit(false)
	, mDevice(opts.Device)
	, mIteration(0)
{
	if (!mManager.init())
		throw std::runtime_error("Could not init modules!");

	LoaderOptions lopts;
	lopts.FilePath		= path;
	lopts.Configuration = targetToConfiguration(opts.DesiredTarget);

	// Parse scene file
	Parser::SceneParser parser;
	bool ok = false;
	lopts.Scene = parser.loadFromFile(path, ok);
	if (!ok)
		throw std::runtime_error("Could not parse scene!");

	// Extract technique
	const auto technique = lopts.Scene.technique();
	if (technique && technique->pluginType() == "debug")
		lopts.Configuration |= IG_C_RENDERER_DEBUG;
	else
		lopts.Configuration |= IG_C_RENDERER_PATH;

	// Extract film
	const auto film = lopts.Scene.film();
	if (film) {
		const auto filmSize				 = film->property("size").getVector2(Vector2f(mLoadedRenderSettings.FilmWidth, mLoadedRenderSettings.FilmHeight));
		mLoadedRenderSettings.FilmWidth	 = filmSize.x();
		mLoadedRenderSettings.FilmHeight = filmSize.y();
	}

	// Extract camera
	const auto camera = lopts.Scene.camera();
	if (camera) {
		lopts.Configuration |= IG_C_CAMERA_PERSPECTIVE; // TODO

		const Transformf t = camera->property("transform").getTransform();

		mLoadedRenderSettings.CameraEye = t * Vector3f::Zero();
		mLoadedRenderSettings.CameraEye = t.linear().col(2);
		mLoadedRenderSettings.CameraEye = t.linear().col(1);

		mLoadedRenderSettings.FOV = camera->property("fov").getNumber(mLoadedRenderSettings.FOV);
	}

	// Check configuration
	const uint64 newConfig = mManager.checkConfiguration(lopts.Configuration);
	if (newConfig != lopts.Configuration) {
		IG_LOG(L_WARNING) << "Switched from "
						  << configurationToString(lopts.Configuration) << " to "
						  << configurationToString(newConfig) << std::endl;
	}
	lopts.Configuration = newConfig;

	LoaderResult result;
	if (!Loader::load(lopts, result))
		throw std::runtime_error("Could not load scene!");
	mDatabase = std::move(result.Database);

	IG_LOG(L_INFO) << "Loading configuration " << configurationToString(newConfig) << std::endl;
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