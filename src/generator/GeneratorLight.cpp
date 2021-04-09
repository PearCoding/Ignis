#include "GeneratorLight.h"

#include "Logger.h"

namespace IG {
static void light_point(const std::shared_ptr<Loader::Object>& light, const GeneratorContext& ctx, std::ostream& os)
{
	auto pos = light->property("position").getVector3();
	os << "make_point_light(math, make_vec3("
	   << pos(0) << ", " << pos(1) << ", " << pos(2) << "), "
	   << ctx.extractMaterialPropertyColor(light, "intensity", 1.0f) << ")";
}

static void light_area(const std::shared_ptr<Loader::Object>& light, const GeneratorContext& ctx, std::ostream& os)
{
	IG_LOG(L_WARNING) << "Area emitter without a shape is not allowed" << std::endl;
	os << "make_point_light(math, make_vec3(0,0,0), pink)/* Inv. Area */";
}

static void light_directional(const std::shared_ptr<Loader::Object>& light, const GeneratorContext& ctx, std::ostream& os)
{
	auto dir = light->property("direction").getVector3();
	os << "make_directional_light(math, make_vec3("
	   << dir(0) << ", " << dir(1) << ", " << dir(2) << "), "
	   << ctx.Environment.SceneDiameter << ", "
	   << ctx.extractMaterialPropertyColor(light, "irradiance", 1.0f) << ")";
}

static void light_sun(const std::shared_ptr<Loader::Object>& light, const GeneratorContext& ctx, std::ostream& os)
{
	auto dir		= light->property("sun_direction").getVector3();
	auto power		= light->property("sun_scale").getNumber(1.0f);
	auto sun_radius = light->property("sun_radius_scale").getNumber(1.0f);
	os << "make_sun_light(math, make_vec3("
	   << dir(0) << ", " << dir(1) << ", " << dir(2) << "), "
	   << ctx.Environment.SceneDiameter << ", "
	   << sun_radius << ", "
	   << "make_gray_color(" << power << "))";
}

static void light_sunsky(const std::shared_ptr<Loader::Object>& light, const GeneratorContext& ctx, std::ostream& os)
{
	auto dir		= light->property("sun_direction").getVector3();
	auto sun_power	= light->property("sun_scale").getNumber(1.0f);
	auto sky_power	= light->property("sky_scale").getNumber(1.0f);
	auto sun_radius = light->property("sun_radius_scale").getNumber(10.0f);
	os << "make_sunsky_light(math, make_vec3("
	   << dir(0) << ", " << dir(1) << ", " << dir(2) << "), "
	   << ctx.Environment.SceneDiameter << ", "
	   << sun_radius << ", "
	   << "make_gray_color(" << sun_power << "), "
	   << "make_gray_color(" << sky_power << "))";
}

static void light_cie_uniform(const std::shared_ptr<Loader::Object>& light, const GeneratorContext& ctx, std::ostream& os)
{
	auto zenith			  = ctx.extractMaterialPropertyColor(light, "zenith", 1.0f);
	auto ground			  = ctx.extractMaterialPropertyColor(light, "ground", 1.0f);
	auto groundbrightness = light->property("ground_brightness").getNumber(0.2f);
	os << "make_cie_uniform_light(math, "
	   << ctx.Environment.SceneDiameter << ", "
	   << zenith << ", "
	   << ground << ", "
	   << groundbrightness << ")";
}

static void light_cie_cloudy(const std::shared_ptr<Loader::Object>& light, const GeneratorContext& ctx, std::ostream& os)
{
	auto zenith			  = ctx.extractMaterialPropertyColor(light, "zenith", 1.0f);
	auto ground			  = ctx.extractMaterialPropertyColor(light, "ground", 1.0f);
	auto groundbrightness = light->property("ground_brightness").getNumber(0.2f);
	os << "make_cie_cloudy_light(math, "
	   << ctx.Environment.SceneDiameter << ", "
	   << zenith << ", "
	   << ground << ", "
	   << groundbrightness << ")";
}

static void light_env(const std::shared_ptr<Loader::Object>& light, const GeneratorContext& ctx, std::ostream& os)
{
	bool isTexture = false;
	auto radiance  = ctx.extractMaterialPropertyColorLight(light, "radiance", 1.0f, isTexture);
	if (isTexture)
		os << "make_environment_light_textured(math, " << ctx.Environment.SceneDiameter << ", " << radiance << ")";
	else
		os << "make_environment_light(math, " << ctx.Environment.SceneDiameter << ", " << radiance << ")";
}

static void light_error(const std::string& msg, std::ostream& os)
{
	IG_LOG(L_ERROR) << msg << std::endl;
	os << "make_point_light(math, make_vec3(0,0,0), pink)/* " << msg << " */";
}

static void light_unknown(const std::shared_ptr<Loader::Object>& light, const GeneratorContext&, std::ostream& os)
{
	IG_LOG(L_WARNING) << "Unknown emitter type '" << light->pluginType() << "'" << std::endl;
	os << "make_point_light(math, make_vec3(0,0,0), pink)/* Unknown */";
}

using LightGenerator = void (*)(const std::shared_ptr<Loader::Object>& light, const GeneratorContext& ctx, std::ostream& os);
static struct {
	const char* Name;
	LightGenerator Generator;
} _generators[] = {
	{ "point", light_point },
	{ "area", light_area },
	{ "directional", light_directional },
	{ "direction", light_directional },
	{ "sun", light_sun },
	{ "sunsky", light_sunsky },
	{ "sky", light_cie_uniform },
	{ "cie_uniform", light_cie_uniform },
	{ "cieuniform", light_cie_uniform },
	{ "cie_cloudy", light_cie_cloudy },
	{ "ciecloudy", light_cie_cloudy },
	{ "uniform", light_env },
	{ "env", light_env },
	{ "envmap", light_env },
	{ "constant", light_env },
	{ "", nullptr }
};

std::string GeneratorLight::extract(const std::shared_ptr<Loader::Object>& light, const GeneratorContext& ctx)
{
	std::stringstream sstream;

	if (!light) {
		light_error("No light given", sstream);
	} else {
		for (size_t i = 0; _generators[i].Generator; ++i) {
			if (_generators[i].Name == light->pluginType()) {
				_generators[i].Generator(light, ctx, sstream);
				return sstream.str();
			}
		}
		light_unknown(light, ctx, sstream);
	}

	return sstream.str();
}

} // namespace IG