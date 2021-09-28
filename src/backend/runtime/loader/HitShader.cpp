#include "HitShader.h"
#include "Loader.h"
#include "LoaderLight.h"
#include "LoaderTechnique.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string HitShader::setup(int entity_id, LoaderContext& ctx, LoaderResult& result)
{
	IG_UNUSED(entity_id);
	
	std::stringstream stream;

	stream << "#[export] fn ig_hit_shader(settings: &Settings, entity_id: i32, first: i32, last: i32) -> () {" << std::endl
		   << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl
		   << std::endl;

	if (LoaderTechnique::requireLights(ctx))
		stream << LoaderLight::generate(ctx, result);

	stream << "  let dtb  = device.load_scene_database();" << std::endl
		   << "  let acc  = SceneAccessor {" << std::endl
		   << "    info       = " << ShaderUtils::generateSceneInfoInline(ctx) << "," << std::endl
		   << "    shapes     = device.load_shape_table(dtb.shapes)," << std::endl
		   << "    entities   = device.load_entity_table(dtb.entities)," << std::endl
		   << "  };" << std::endl
		   << std::endl;

	stream << "  let scene = Scene {" << std::endl
		   << "    info     = acc.info," << std::endl
		   << "    database = acc" << std::endl
		   << "  };" << std::endl
		   << std::endl;

	// TODO: Setup proper shading tree
	stream << "  let shader : Shader = @|_ray, _hit, surf| make_material(make_diffuse_bsdf(surf, white));" << std::endl
		   << std::endl;

	stream << LoaderTechnique::generate(ctx, result) << std::endl
		   << std::endl;

	stream << "  let spp = 4 : i32;" << std::endl // TODO ?
		   << "  device.handle_hit_shader(entity_id, shader, scene, technique, first, last, spp);" << std::endl
		   << "}" << std::endl;

	return stream.str();
}

} // namespace IG