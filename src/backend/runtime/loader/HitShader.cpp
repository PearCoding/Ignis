#include "HitShader.h"
#include "Loader.h"
#include "LoaderTechnique.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string HitShader::setup(int entity_id, LoaderContext& ctx, LoaderResult& result)
{
	std::stringstream stream;

	stream << "#[export] fn ig_main(settings: &Settings, entity_id: i32, first: i32, last: i32) -> () {" << std::endl
		   << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl
		   << std::endl;

	// TODO: Setup lights

	stream << "  let dtb  = device.load_scene_database();" << std::endl
		   << "  let acc  = SceneAccessor {" << std::endl
		   << "    info       = device.load_scene_info()," << std::endl
		   << "    shapes     = device.load_shape_table(dtb.shapes)," << std::endl
		   << "    entities   = device.load_entity_table(dtb.entities)," << std::endl
		   << "    lights     = make_null_light_table()," << std::endl
		   << "    arealights = make_null_light_table()" << std::endl
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

	stream << "  let technique = " << LoaderTechnique::generate(false, ctx, result) << ";" << std::endl
		   << std::endl;

	stream << "  let spp = 4 : i32;" << std::endl // TODO ?
		   << "  device.handle_hit_shader(entity_id, shader, scene, technique, first, last, spp);" << std::endl
		   << "}" << std::endl;

	return stream.str();
}

} // namespace IG