#include "HitShader.h"
#include "Loader.h"
#include "LoaderBSDF.h"
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
		   << "  maybe_unused(settings);" << std::endl
		   << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl
		   << std::endl;

	stream << "  let dtb      = device.load_scene_database();" << std::endl
		   << "  let shapes   = device.load_shape_table(dtb.shapes);" << std::endl
		   << "  let entities = device.load_entity_table(dtb.entities);" << std::endl
		   << std::endl;

	// FIXME: Area lights require preloaded entity and shape information
	// which have to be loaded prior a GPU kernel
	if (isCPU(ctx.Target)) {
		stream << "  let cpu_shapes   = shapes;" << std::endl
			   << "  let cpu_entities = entities;" << std::endl
			   << std::endl;
	} else {
		stream << "  let cpu_dev      = make_cpu_default_device();" << std::endl
			   << "  let cpu_dtb      = cpu_dev.load_scene_database();" << std::endl
			   << "  let cpu_shapes   = cpu_dev.load_shape_table(cpu_dtb.shapes);" << std::endl
			   << "  let cpu_entities = cpu_dev.load_entity_table(cpu_dtb.entities);" << std::endl
			   << std::endl;
	}

	if (LoaderTechnique::requireLights(ctx))
		stream << LoaderLight::generate(ctx, false) << std::endl
			   << std::endl;

	stream << "  let acc  = SceneAccessor {" << std::endl
		   << "    info     = " << ShaderUtils::generateSceneInfoInline(ctx) << "," << std::endl
		   << "    shapes   = shapes," << std::endl
		   << "    entities = entities," << std::endl
		   << "  };" << std::endl
		   << std::endl;

	stream << "  let scene = Scene {" << std::endl
		   << "    info     = acc.info," << std::endl
		   << "    database = acc" << std::endl
		   << "  };" << std::endl
		   << std::endl;

	const std::string bsdf_name = ctx.Environment.Entities[entity_id].BSDF;
	stream << LoaderBSDF::generate(bsdf_name, ctx);

	const std::string entity_name = ctx.Environment.Entities[entity_id].Name;
	const bool isLight			  = ctx.Environment.AreaLightsMap.count(entity_name) > 0;

	if (isLight) {
		const std::string light_name = ctx.Environment.AreaLightsMap[entity_name];
		stream << "  let shader : Shader = @|ray, hit, surf| make_emissive_material(surf, bsdf_" << ShaderUtils::escapeIdentifier(bsdf_name) << "(ray, hit, surf), "
			   << "light_" << ShaderUtils::escapeIdentifier(light_name) << ");" << std::endl
			   << std::endl;
	} else {
		stream << "  let shader : Shader = @|ray, hit, surf| make_material(bsdf_" << ShaderUtils::escapeIdentifier(bsdf_name) << "(ray, hit, surf));" << std::endl
			   << std::endl;
	}

	stream << LoaderTechnique::generate(ctx, result) << std::endl
		   << std::endl;

	stream << "  let spp = 4 : i32;" << std::endl // TODO ?
		   << "  device.handle_hit_shader(entity_id, shader, scene, technique, first, last, spp);" << std::endl
		   << "}" << std::endl;

	return stream.str();
}

} // namespace IG