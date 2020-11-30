#include "GeneratorLight.h"

#include "Logger.h"

namespace IG {
std::string GeneratorLight::extract(const std::shared_ptr<Loader::Object>& light, const GeneratorContext& ctx)
{
	std::stringstream sstream;

	if (light->pluginType() == "point") {
		auto pos = light->property("position").getVector3();
		sstream << "make_point_light(math, make_vec3("
				<< pos(0) << ", " << pos(1) << ", " << pos(2) << "), "
				<< ctx.extractMaterialPropertyColor(light, "intensity", 1.0f) << ")";
	} else if (light->pluginType() == "area") {
		IG_LOG(L_WARNING) << "Area emitter without a shape is not allowed" << std::endl;
		sstream << "make_point_light(math, make_vec3(0,0,0), pink)/* Inv. Area */";
	} else if (light->pluginType() == "directional") { // TODO: By to_world?
		auto dir = light->property("direction").getVector3();
		sstream << "make_directional_light(math, make_vec3("
				<< dir(0) << ", " << dir(1) << ", " << dir(2) << "), "
				<< ctx.Environment.SceneDiameter << ", "
				<< ctx.extractMaterialPropertyColor(light, "irradiance", 1.0f) << ")";
	} else if (light->pluginType() == "sun" || light->pluginType() == "sunsky") { // TODO
		IG_LOG(L_WARNING) << "Sun emitter is approximated by directional light" << std::endl;
		auto dir   = light->property("sun_direction").getVector3();
		auto power = light->property("scale").getNumber(1.0f);
		sstream << "make_directional_light(math, make_vec3("
				<< dir(0) << ", " << dir(1) << ", " << dir(2) << "), "
				<< ctx.Environment.SceneDiameter << ", "
				<< "make_gray_color(" << power << "))";
	} else if (light->pluginType() == "constant" || light->pluginType() == "envmap") {
		sstream << "make_environment_light(math, "
				<< ctx.Environment.SceneDiameter << ", "
				<< ctx.extractMaterialPropertyColor(light, "radiance", 1.0f) << ")";
	} else {
		IG_LOG(L_WARNING) << "Unknown emitter type '" << light->pluginType() << "'" << std::endl;
		sstream << "make_point_light(math, make_vec3(0,0,0), pink)/* Unknown */";
	}

	return sstream.str();
}

} // namespace IG