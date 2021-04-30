#pragma once

#include "IG_Config.h"

namespace IG {
struct SceneDatabase;
}

// Not in namespace IG
struct DriverSetupSettings {
	IG::uint32 framebuffer_width;
	IG::uint32 framebuffer_height;
	IG::SceneDatabase* database;
};

struct DriverRay {
	float origin[3];
	float dir[3];
	float tmin;
	float tmax;
};

struct DriverRenderSettings {
	float eye[3];
	float dir[3];
	float up[3];
	float right[3];
	float width;
	float height;
	IG::uint32 ray_count;
	DriverRay* rays;
	IG::uint32 device;
};

using DriverRenderFunction			 = void (*)(const DriverRenderSettings*, IG::uint32);
using DriverShutdownFunction		 = void (*)();
using DriverSetupFunction			 = void (*)(const DriverSetupSettings*);
using DriverGetFramebufferFunction	 = const float* (*)(int);
using DriverClearFramebufferFunction = void (*)(int);

struct DriverInterface {
	IG::uint32 SPP;
	const char* Name;
	IG::uint64 Configuration;
	DriverSetupFunction SetupFunction;
	DriverShutdownFunction ShutdownFunction;
	DriverRenderFunction RenderFunction;
	DriverGetFramebufferFunction GetFramebufferFunction;
	DriverClearFramebufferFunction ClearFramebufferFunction;
};