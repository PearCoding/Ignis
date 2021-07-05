#include "Configuration.h"

namespace IG {
std::string configurationToString(uint64 config)
{
	std::stringstream stream;

	if (config & IG_C_DEVICE_GENERIC)
		stream << "[Generic]";
	if (config & IG_C_DEVICE_ASIMD)
		stream << "[ASIMD]";
	if (config & IG_C_DEVICE_SSE42)
		stream << "[SSE4.2]";
	if (config & IG_C_DEVICE_AVX)
		stream << "[AVX]";
	if (config & IG_C_DEVICE_AVX2)
		stream << "[AVX2]";
	if (config & IG_C_DEVICE_AVX512)
		stream << "[AVX512]";
	if (config & IG_C_DEVICE_NVVM)
		stream << "[NVVM]";
	if (config & IG_C_DEVICE_NVVM_MEGA)
		stream << "[NVVM_Megakernel]";
	if (config & IG_C_DEVICE_AMDGPU)
		stream << "[AMD]";
	if (config & IG_C_DEVICE_AMD_MEGA)
		stream << "[AMD_Megakernel]";

	if (config & IG_C_CAMERA_PERSPECTIVE)
		stream << "[Perspective]";
	if (config & IG_C_CAMERA_ORTHOGONAL)
		stream << "[Orthogonal]";
	if (config & IG_C_CAMERA_FISHLENS)
		stream << "[Fishlens]";
	if (config & IG_C_CAMERA_LIST)
		stream << "[List]";

	if (config & IG_C_RENDERER_PATH)
		stream << "[Path]";
	if (config & IG_C_RENDERER_DEBUG)
		stream << "[Debug]";
	if (config & IG_C_RENDERER_AO)
		stream << "[AO]";

	if (config & IG_C_NO_INSTANCES)
		stream << "[NoInstances]";

	return stream.str();
}
} // namespace IG