#include "LoaderLight.h"
#include "Loader.h"
#include "Logger.h"
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

std::string LoaderLight::generate(LoaderContext& ctx, LoaderResult& result)
{
	std::stringstream stream;

	// TODO
	stream << "  let num_lights = 1;" << std::endl
		   << "  let lights = @|id:i32| { " << std::endl
		   << "    match(id) {" << std::endl
		   << "      _ => make_environment_light(" << ctx.Environment.SceneDiameter / 2 << ", white)" << std::endl
		   << "    }" << std::endl
		   << "  };" << std::endl;

	return stream.str();
}

} // namespace IG