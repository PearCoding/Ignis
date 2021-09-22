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
	if (config & IG_C_DEVICE_AMDGPU)
		stream << "[AMD]";

	if (config & IG_C_NO_INSTANCES)
		stream << "[NoInstances]";

	return stream.str();
}
} // namespace IG