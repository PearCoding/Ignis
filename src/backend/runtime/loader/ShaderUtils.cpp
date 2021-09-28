#include "ShaderUtils.h"

#include <sstream>

namespace IG {
std::string ShaderUtils::constructDevice(Target target)
{
	std::stringstream stream;

	switch (target) {
	case Target::AVX:
		stream << "let device = make_avx_device();";
		break;
	case Target::AVX2:
		stream << "let device = make_avx2_device();";
		break;
	case Target::AVX512:
		stream << "let device = make_avx512_device();";
		break;
	case Target::SSE42:
		stream << "let device = make_sse42_device();";
		break;
	case Target::ASIMD:
		stream << "let device = make_asimd_device();";
		break;
	case Target::NVVM:
		stream << "let device = make_nvvm_device(settings.device);";
		break;
	case Target::AMDGPU:
		stream << "let device = make_amdgpu_device(settings.device);";
		break;
	default:
		stream << "let device = make_cpu_default_device();";
		break;
	}

	return stream.str();
}

std::string ShaderUtils::generateSceneInfoInline(const LoaderContext& ctx)
{
	std::stringstream stream;

	stream << "SceneInfo { num_entities = " << ctx.Environment.EntityIDs.size() << ", num_shapes = " << ctx.Environment.ShapeIDs.size() << " }";

	return stream.str();
}
} // namespace IG