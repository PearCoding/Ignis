#include "LoaderLight.h"
#include "Logger.h"
#include "skysun/SkyModel.h"
#include "skysun/SunLocation.h"

namespace IG {

static ElevationAzimuth extractEA(const std::shared_ptr<Parser::Object>& obj)
{
	if (obj->property("direction").isValid()) {
		return ElevationAzimuth::fromDirection(obj->property("direction").getVector3(Vector3f(0, 0, 1)));
	} else if (obj->property("sun_direction").isValid()) {
		return ElevationAzimuth::fromDirection(obj->property("sun_direction").getVector3(Vector3f(0, 0, 1)));
	} else if (obj->property("theta").isValid() || obj->property("phi").isValid()) {
		return ElevationAzimuth::fromThetaPhi(obj->property("theta").getNumber(0), obj->property("phi").getNumber(0));
	} else if (obj->property("elevation").isValid() || obj->property("azimuth").isValid()) {
		return ElevationAzimuth{ obj->property("elevation").getNumber(0), obj->property("azimuth").getNumber(0) };
	} else {
		TimePoint timepoint;
		MapLocation location;
		timepoint.Year	   = obj->property("year").getInteger(timepoint.Year);
		timepoint.Month	   = obj->property("month").getInteger(timepoint.Month);
		timepoint.Day	   = obj->property("day").getInteger(timepoint.Day);
		timepoint.Hour	   = obj->property("hour").getInteger(timepoint.Hour);
		timepoint.Minute   = obj->property("minute").getInteger(timepoint.Minute);
		timepoint.Seconds  = obj->property("seconds").getNumber(timepoint.Seconds);
		location.Latitude  = obj->property("latitude").getNumber(location.Latitude);
		location.Longitude = obj->property("longitude").getNumber(location.Longitude);
		location.Timezone  = obj->property("timezone").getNumber(location.Timezone);
		return computeSunEA(timepoint, location);
	}
}

/*static void setup_sky(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
	auto ground	   = light->property("ground").getVector3(Vector3f(0.8f, 0.8f, 0.8f));
	auto turbidity = light->property("turbidity").getNumber(3.0f);
	auto ea		   = extractEA(light);

	SkyModel model(ground, ea, turbidity);
	model.save("data/skytex_" + LoaderContext::makeId(name) + ".exr");
}*/

static void light_point(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx, std::ostream& os)
{
	auto pos = light->property("position").getVector3();
	os << "make_point_light(math, make_vec3("
	   << pos(0) << ", " << pos(1) << ", " << pos(2) << "), "
	   << ctx.extractMaterialPropertyColor(light, "intensity", 1.0f) << ")";
}

static void light_area(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx, std::ostream& os)
{
	IG_LOG(L_WARNING) << "Area emitter without a shape is not allowed" << std::endl;
	os << "make_point_light(math, make_vec3(0,0,0), pink)/* Inv. Area */";
}

static void light_directional(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx, std::ostream& os)
{
	auto ea		 = extractEA(light);
	Vector3f dir = ea.toDirection();

	os << "make_directional_light(math, make_vec3("
	   << dir(0) << ", " << dir(1) << ", " << dir(2) << "), "
	   << ctx.Environment.SceneDiameter << ", "
	   << ctx.extractMaterialPropertyColor(light, "irradiance", 1.0f) << ")";
}

static void light_sun(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx, std::ostream& os)
{
	auto ea		 = extractEA(light);
	Vector3f dir = ea.toDirection();

	auto power		= light->property("sun_scale").getNumber(1.0f);
	auto sun_radius = light->property("sun_radius_scale").getNumber(1.0f);
	os << "make_sun_light(math, make_vec3("
	   << dir(0) << ", " << dir(1) << ", " << dir(2) << "), "
	   << ctx.Environment.SceneDiameter << ", "
	   << sun_radius << ", "
	   << "make_gray_color(" << power << "))";
}

/*static void light_sky(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx, std::ostream& os)
{
	setup_sky(name, light, ctx);

	std::string tex_path = "skytex_" + LoaderContext::makeId(name);
	os << "make_environment_light_textured(math, " << ctx.Environment.SceneDiameter << ", " << tex_path << ")";
}

// TODO: Why not just add two lights instead of this combination?
static void light_sunsky(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx, std::ostream& os)
{
	setup_sky(name, light, ctx);
	const std::string tex_path = "skytex_" + LoaderContext::makeId(name);

	auto ea		 = extractEA(light);
	Vector3f dir = ea.toDirection();

	auto sun_power	= light->property("sun_scale").getNumber(1.0f);
	auto sun_radius = light->property("sun_radius_scale").getNumber(10.0f);
	os << "make_sunsky_light(math, make_vec3("
	   << dir(0) << ", " << dir(1) << ", " << dir(2) << "), "
	   << ctx.Environment.SceneDiameter << ", "
	   << sun_radius << ", "
	   << "make_gray_color(" << sun_power << "), "
	   << tex_path << ")";
}*/

static void light_cie_uniform(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx, std::ostream& os)
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

static void light_cie_cloudy(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx, std::ostream& os)
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

static inline float perez_model(float zenithAngle, float sunAngle, float a, float b, float c, float d, float e)
{
	float zc = std::cos(zenithAngle);
	float sc = std::cos(sunAngle);

	float A = 1 + a * std::exp(b * zc);
	float B = 1 + c * std::exp(d * sunAngle) + e * sc * sc;
	return A * B;
}

static void light_perez(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx, std::ostream& os)
{
	auto ea		 = extractEA(light);
	Vector3f dir = ea.toDirection();

	auto a = light->property("a").getNumber(1.0f);
	auto b = light->property("b").getNumber(1.0f);
	auto c = light->property("c").getNumber(1.0f);
	auto d = light->property("d").getNumber(1.0f);
	auto e = light->property("e").getNumber(1.0f);

	os << "make_perez_light(math, "
	   << ctx.Environment.SceneDiameter << ", "
	   << "make_vec3(" << dir(0) << ", " << dir(1) << ", " << dir(2) << "), ";

	if (light->properties().count("luminance")) {
		auto lum = ctx.extractMaterialPropertyColor(light, "luminance", 1.0f);
		os << lum << ", ";
	} else {
		auto zenith			= ctx.extractMaterialPropertyColor(light, "zenith", 1.0f);
		const float groundZ = perez_model(0, -dir(2), a, b, c, d, e); // TODO: Validate
		os << "color_mulf(" << zenith << ", " << groundZ << "), ";
	}

	os << a << ", "
	   << b << ", "
	   << c << ", "
	   << d << ", "
	   << e << ")";
}

static void light_env(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx, std::ostream& os)
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

static void light_unknown(const std::shared_ptr<Parser::Object>& light, const LoaderContext&, std::ostream& os)
{
	IG_LOG(L_WARNING) << "Unknown emitter type '" << light->pluginType() << "'" << std::endl;
	os << "make_point_light(math, make_vec3(0,0,0), pink)/* Unknown */";
}

using LightLoader = void (*)(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx, std::ostream& os);
static struct {
	const char* Name;
	LightLoader Loader;
} _generators[] = {
	{ "point", light_point },
	{ "area", light_area },
	{ "directional", light_directional },
	{ "direction", light_directional },
	{ "sun", light_sun },
	/*{ "sunsky", light_sunsky },
	{ "skysun", light_sunsky },
	{ "sky", light_sky },*/
	{ "cie_uniform", light_cie_uniform },
	{ "cieuniform", light_cie_uniform },
	{ "cie_cloudy", light_cie_cloudy },
	{ "ciecloudy", light_cie_cloudy },
	{ "perez", light_perez },
	{ "uniform", light_env },
	{ "env", light_env },
	{ "envmap", light_env },
	{ "constant", light_env },
	{ "", nullptr }
};

std::string LoaderLight::extract(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
	std::stringstream sstream;

	if (!light) {
		light_error("No light given", sstream);
	} else {
		for (size_t i = 0; _generators[i].Loader; ++i) {
			if (_generators[i].Name == light->pluginType()) {
				_generators[i].Loader(name, light, ctx, sstream);
				return sstream.str();
			}
		}
		light_unknown(light, ctx, sstream);
	}

	return sstream.str();
}

} // namespace IG