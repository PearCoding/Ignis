#pragma once

#include "Target.h"
#include <vector>

namespace IG {
struct SceneDatabase;
struct Ray;
class Statistics;
} // namespace IG

// Not in namespace IG
struct DriverSetupSettings {
	IG::uint32 framebuffer_width;
	IG::uint32 framebuffer_height;
	IG::SceneDatabase* database;
	void* ray_generation_shader;
	void* miss_shader;
	std::vector<void*> hit_shaders;
	bool acquire_stats = false;
};

struct DriverRenderSettings {
	float eye[3];
	float dir[3];
	float up[3];
	float right[3];
	float width;
	float height;
	float tmin;
	float tmax;
	const IG::Ray* rays; // If non-null, width contains the number of rays and height is set to 1
	IG::uint32 device;
	IG::uint32 spi;
	IG::uint32 max_path_length;
	IG::uint32 debug_mode; // Only useful for debug integrator
};

using DriverRenderFunction			 = void (*)(const DriverRenderSettings*, IG::uint32);
using DriverShutdownFunction		 = void (*)();
using DriverSetupFunction			 = void (*)(const DriverSetupSettings*);
using DriverGetFramebufferFunction	 = const float* (*)(int);
using DriverClearFramebufferFunction = void (*)(int);
using DriverGetStatisticsFunction	 = const IG::Statistics* (*)();

struct DriverInterface {
	IG::uint32 MajorVersion;
	IG::uint32 MinorVersion;
	const char* Name;
	IG::Target Target;
	DriverSetupFunction SetupFunction;
	DriverShutdownFunction ShutdownFunction;
	DriverRenderFunction RenderFunction;
	DriverGetFramebufferFunction GetFramebufferFunction;
	DriverClearFramebufferFunction ClearFramebufferFunction;
	DriverGetStatisticsFunction GetStatisticsFunction;
};