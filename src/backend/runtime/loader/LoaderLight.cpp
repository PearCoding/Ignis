#include "LoaderLight.h"
#include "Loader.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "serialization/VectorSerializer.h"
#include "skysun/SkyModel.h"
#include "skysun/SunLocation.h"

#include <chrono>

namespace IG {

static ElevationAzimuth extractEA(const std::shared_ptr<Parser::Object>& obj)
{
	if (obj->property("direction").isValid()) {
		return ElevationAzimuth::fromDirection(obj->property("direction").getVector3(Vector3f(0, 0, 1)).normalized());
	} else if (obj->property("sun_direction").isValid()) {
		return ElevationAzimuth::fromDirection(obj->property("sun_direction").getVector3(Vector3f(0, 0, 1)).normalized());
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

static uint32 setup_sky(const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
	IG_UNUSED(ctx);

	auto ground	   = light->property("ground").getVector3(Vector3f(0.8f, 0.8f, 0.8f));
	auto turbidity = light->property("turbidity").getNumber(3.0f);
	auto ea		   = extractEA(light);

	SkyModel model(ground, ea, turbidity);

	// TODO

	return 0;
}

static std::string light_point(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
	IG_UNUSED(name);

	auto pos	   = light->property("position").getVector3();
	auto intensity = ctx.extractColor(light, "intensity");

	std::stringstream stream;
	stream << "make_point_light(" << ShaderUtils::inlineVector(pos)
		   << ", " << ShaderUtils::inlineColor(intensity) << ")";
	return stream.str();
}

static std::string light_area(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
	IG_UNUSED(name);

	const std::string entity = light->property("entity").getString();

	uint32 entity_id = 0;
	if (!ctx.Environment.EntityIDs.count(entity))
		IG_LOG(L_ERROR) << "No entity named '" << entity << "' exists for area light" << std::endl;
	else
		entity_id = ctx.Environment.EntityIDs.at(entity);

	IG_UNUSED(entity_id);
	// TODO
	return "";
}

static std::string light_directional(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
	IG_UNUSED(name);

	auto ea			= extractEA(light);
	Vector3f dir	= ea.toDirection();
	auto irradiance = ctx.extractColor(light, "irradiance");

	std::stringstream stream;
	stream << "make_directional_light(" << ShaderUtils::inlineVector(dir)
		   << ", " << ctx.Environment.SceneDiameter / 2
		   << ", " << ShaderUtils::inlineColor(irradiance) << ")";
	return stream.str();
}

static std::string light_sun(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
	IG_UNUSED(name);
	IG_UNUSED(ctx);

	auto ea		 = extractEA(light);
	Vector3f dir = ea.toDirection();

	auto power		= light->property("sun_scale").getNumber(1.0f);
	auto sun_radius = light->property("sun_radius_scale").getNumber(1.0f);

	std::stringstream stream;
	stream << "make_sun_light(" << ShaderUtils::inlineVector(dir)
		   << ", " << ctx.Environment.SceneDiameter / 2
		   << ", " << sun_radius
		   << ", color_mulf(white, " << power << "))";
	return stream.str();
}

static std::string light_sky(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
	IG_UNUSED(name);

	setup_sky(light, ctx);
	// TODO
	return "";
}

static std::string light_sunsky(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
	light_sky(name + "_sky", light, ctx);
	light_sun(name + "_sun", light, ctx);
	// TODO
	return "";
}

static std::string light_cie_uniform(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
	IG_UNUSED(name);

	auto zenith			  = ctx.extractColor(light, "zenith");
	auto ground			  = ctx.extractColor(light, "ground");
	auto groundbrightness = light->property("ground_brightness").getNumber(0.2f);

	std::stringstream stream;
	stream << "make_cie_sky_light(" << ctx.Environment.SceneDiameter / 2
		   << ", " << ShaderUtils::inlineColor(zenith)
		   << ", " << ShaderUtils::inlineColor(ground)
		   << ", " << groundbrightness
		   << ", false)";
	return stream.str();
}

static std::string light_cie_cloudy(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
	IG_UNUSED(name);

	auto zenith			  = ctx.extractColor(light, "zenith");
	auto ground			  = ctx.extractColor(light, "ground");
	auto groundbrightness = light->property("ground_brightness").getNumber(0.2f);

	std::stringstream stream;
	stream << "make_cie_sky_light(" << ctx.Environment.SceneDiameter / 2
		   << ", " << ShaderUtils::inlineColor(zenith)
		   << ", " << ShaderUtils::inlineColor(ground)
		   << ", " << groundbrightness
		   << ", false)";
	return stream.str();
}

static inline float perez_model(float zenithAngle, float sunAngle, float a, float b, float c, float d, float e)
{
	float zc = std::cos(zenithAngle);
	float sc = std::cos(sunAngle);

	float A = 1 + a * std::exp(b * zc);
	float B = 1 + c * std::exp(d * sunAngle) + e * sc * sc;
	return A * B;
}

static std::string light_perez(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
	IG_UNUSED(name);

	auto ea		 = extractEA(light);
	Vector3f dir = ea.toDirection();

	auto a = light->property("a").getNumber(1.0f);
	auto b = light->property("b").getNumber(1.0f);
	auto c = light->property("c").getNumber(1.0f);
	auto d = light->property("d").getNumber(1.0f);
	auto e = light->property("e").getNumber(1.0f);

	Vector3f color;
	if (light->properties().count("luminance")) {
		color = ctx.extractColor(light, "luminance");
	} else {
		auto zenith			= ctx.extractColor(light, "zenith");
		const float groundZ = perez_model(0, -dir(2), a, b, c, d, e); // TODO: Validate
		color				= zenith * groundZ;
	}

	std::stringstream stream;
	stream << "make_perez_light(" << ctx.Environment.SceneDiameter / 2
		   << ", " << ShaderUtils::inlineVector(dir)
		   << ", " << ShaderUtils::inlineColor(color)
		   << ", " << a
		   << ", " << b
		   << ", " << c
		   << ", " << d
		   << ", " << e << ")";
	return stream.str();
}

static std::string light_env(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
	IG_UNUSED(name);

	const Vector3f color = ctx.extractColor(light, "radiance");

	std::stringstream stream;
	stream << "make_environment_light(" << ctx.Environment.SceneDiameter / 2
		   << ", " << ShaderUtils::inlineColor(color) << ")";
	return stream.str();
}

using LightLoader = std::string (*)(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx);
static struct {
	const char* Name;
	LightLoader Loader;
} _generators[] = {
	{ "point", light_point },
	{ "area", light_area },
	{ "directional", light_directional },
	{ "direction", light_directional },
	{ "sun", light_sun },
	{ "sunsky", light_sunsky },
	{ "skysun", light_sunsky },
	{ "sky", light_sky },
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

std::string LoaderLight::generate(LoaderContext& ctx, LoaderResult& result)
{
	IG_UNUSED(result);

	std::stringstream stream;

	stream << "  let lights = @|id:i32| {" << std::endl
		   << "    match(id) {" << std::endl;

	size_t counter = 0;
	for (const auto& pair : ctx.Scene.lights()) {
		const auto light = pair.second;

		if (light->pluginType() == "area"
			|| light->pluginType() == "sunsky"
			|| light->pluginType() == "skysun"
			|| light->pluginType() == "sky")
			continue; // FIXME: Skip for now

		bool found = false;
		for (size_t i = 0; _generators[i].Loader; ++i) {
			if (_generators[i].Name == light->pluginType()) {
				stream << "      " << counter << " => "
					   << _generators[i].Loader(pair.first, light, ctx)
					   << "," << std::endl;
				++counter;
				found = true;
				break;
			}
		}
		if (!found)
			IG_LOG(L_ERROR) << "No light type '" << light->pluginType() << "' available" << std::endl;
	}

	stream << "      _ => make_null_light()" << std::endl
		   << "    }" << std::endl
		   << "  };" << std::endl
		   << "  let num_lights = " << counter << ";" << std::endl;

	return stream.str();
}

} // namespace IG