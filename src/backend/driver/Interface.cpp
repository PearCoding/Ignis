#include "Interface.h"

extern "C" {
IG_EXPORT DriverInterface ig_get_interface()
{
	DriverInterface interface;
	// Expose SPP
#ifdef CAMERA_LIST
	interface.SPP = 1;
#else
	interface.SPP = 4;
#endif

	interface.Configuration = 0;
// Expose Target
#if defined(DEVICE_GENERIC)
	interface.Configuration |= IG_C_DEVICE_GENERIC;
#elif defined(DEVICE_AVX)
	interface.Configuration |= IG_C_DEVICE_AVX;
#elif defined(DEVICE_AVX_2)
	interface.Configuration |= IG_C_DEVICE_AVX2;
#elif defined(DEVICE_SSE42)
	interface.Configuration |= IG_C_DEVICE_SSE42;
#elif defined(DEVICE_ASIMD)
	interface.Configuration |= IG_C_DEVICE_ASIMD;
#elif defined(DEVICE_NVVM)
	interface.Configuration |= IG_C_DEVICE_NVVM_STREAMING;
#elif defined(DEVICE_NVVM_MEGA)
	interface.Configuration |= IG_C_DEVICE_NVVM_MEGA;
#elif defined(DEVICE_AMD)
	interface.Configuration |= IG_C_DEVICE_AMD_STREAMING;
#elif defined(DEVICE_AMD_MEGA)
	interface.Configuration |= IG_C_DEVICE_AMD_MEGA;
#else
#error No device selected!
#endif

// Expose Camera
#if defined(CAMERA_PERSPECTIVE)
	interface.Configuration |= IG_C_CAMERA_PERSPECTIVE;
#elif defined(CAMERA_ORTHOGONAL)
	interface.Configuration |= IG_C_CAMERA_ORTHOGONAL;
#elif defined(CAMERA_FISHLENS)
	interface.Configuration |= IG_C_CAMERA_FISHLENS;
#elif defined(CAMERA_LIST)
	interface.Configuration |= IG_C_CAMERA_LIST;
#else
#error No camera selected!
#endif

// Expose Integrator
#if defined(RENDERER_PATH)
	interface.Configuration |= IG_C_RENDERER_PATH;
#elif defined(RENDERER_DEBUG)
	interface.Configuration |= IG_C_RENDERER_DEBUG;
#else
#error No renderer selected!
#endif

	return interface;
}
}