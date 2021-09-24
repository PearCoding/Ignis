#include "RayGenerationShader.h"
#include "Loader.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string RayGenerationShader::setup(LoaderContext& ctx, LoaderResult& result)
{
	std::stringstream stream;

	stream << "#[export] fn ig_main(settings: &Settings, iter: i32, capacity: i32, id: &mut i32, xmin: i32, ymin: i32, xmax: i32, ymax: i32) -> i32 {" << std::endl;
	stream << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl;
	stream << std::endl;

	std::string gen;
	if (ctx.CameraType == "perspective")
		gen = "make_perspective_camera";
	else if (ctx.CameraType == "orthogonal")
		gen = "make_orthogonal_camera";
	else if (ctx.CameraType == "fishlens")
		gen = "make_fishlens_camera";
	else if (ctx.CameraType != "list") {
		IG_LOG(L_ERROR) << "Unknown camera type '" << ctx.CameraType << "'" << std::endl;
		return {};
	}

	if (!gen.empty()) {
		stream << "  let camera = " << gen << "(" << std::endl
			   << "    settings.eye," << std::endl
			   << "    make_mat3x3(settings.right, settings.up, settings.dir)," << std::endl
			   << "    settings.width," << std::endl
			   << "    settings.height," << std::endl
			   << "    settings.tmin," << std::endl
			   << "    settings.tmax" << std::endl
			   << "  );" << std::endl
			   << std::endl;
	}

	if (ctx.CameraType == "list") {
		stream << "  let emitter = make_list_emitter(device.load_rays(), iter);" << std::endl;
	} else {
		IG_ASSERT(!gen.empty(), "Generator function can not be empty!");
		stream << "  let emitter = make_camera_emitter(camera, iter);" << std::endl;
	}

	stream << "  let spp = 4 : i32;" << std::endl; // TODO ?
	stream << "  device.generate_rays(capacity, emitter, id, xmin, ymin, xmax, ymax, spp)" << std::endl;
	stream << "}" << std::endl;

	return stream.str();
}

} // namespace IG