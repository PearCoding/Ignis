#include "GeneratorLight.h"

#include "Logger.h"

namespace IG {
std::string GeneratorLight::extract(const std::shared_ptr<TPMObject>& light, const GeneratorContext& ctx)
{
	std::stringstream sstream;

	if (light->pluginType() == "point") {
		auto pos = light->property("position").getVector();
		sstream << "make_point_light(math, make_vec3("
				<< pos.x << ", " << pos.y << ", " << pos.z << "), "
				<< ctx.extractMaterialPropertyColor(light, "intensity", 1.0f) << ")";
	} else if (light->pluginType() == "area") {
		IG_LOG(L_WARNING) << "Area emitter without a shape is not allowed" << std::endl;
		sstream << "make_point_light(math, make_vec3(0,0,0), pink)/* Inv. Area */";
	} else if (light->pluginType() == "directional") { // TODO: By to_world?
		auto dir = light->property("direction").getVector();
		sstream << "make_directional_light(math, make_vec3("
				<< dir.x << ", " << dir.y << ", " << dir.z << "), "
				<< ctx.Environment.SceneDiameter << ", "
				<< ctx.extractMaterialPropertyColor(light, "irradiance", 1.0f) << ")";
	} else if (light->pluginType() == "sun" || light->pluginType() == "sunsky") { // TODO
		IG_LOG(L_WARNING) << "Sun emitter is approximated by directional light" << std::endl;
		auto dir   = light->property("sun_direction").getVector();
		auto power = light->property("scale").getNumber(1.0f);
		sstream << "make_directional_light(math, make_vec3("
				<< dir.x << ", " << dir.y << ", " << dir.z << "), "
				<< ctx.Environment.SceneDiameter << ", "
				<< power << "))";
	} else if (light->pluginType() == "constant") {
		sstream << "make_environment_light(math, "
				<< ctx.Environment.SceneDiameter << ", "
				<< ctx.extractMaterialPropertyColor(light, "radiance", 1.0f) << ")";
	} else if (light->pluginType() == "envmap") {
		auto filename = light->property("filename").getString();
		std::string tex_hndl;
		sstream << ctx.generateTextureLoadInstruction(filename, &tex_hndl) << "\n";
		sstream << "make_environment_light_textured(math, "
				<< ctx.Environment.SceneDiameter << ", "
				<< tex_hndl << ")";
	} else {
		IG_LOG(L_WARNING) << "Unknown emitter type '" << light->pluginType() << "'" << std::endl;
		sstream << "make_point_light(math, make_vec3(0,0,0), pink)/* Unknown */";
	}

	return sstream.str();
}

} // namespace IG