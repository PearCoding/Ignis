#include "GeneratorBSDF.h"
#include "Logger.h"

namespace IG {
std::string GeneratorBSDF::extract(const std::shared_ptr<TPMObject>& bsdf, const GeneratorContext& ctx)
{
	std::stringstream sstream;

	const auto error = [&](const std::string& msg) {
		IG_LOG(L_ERROR) << msg << std::endl;
		sstream << "make_black_bsdf()/* ERROR */";
	};

	if (!bsdf) {
		sstream << "make_black_bsdf()";
	} else if (bsdf->pluginType() == "diffuse" || bsdf->pluginType() == "roughdiffuse" /*TODO*/) {
		sstream << "make_diffuse_bsdf(math, surf, " << ctx.extractMaterialPropertyColor(bsdf, "reflectance") << ")";
	} else if (bsdf->pluginType() == "dielectric" || bsdf->pluginType() == "roughdielectric" /*TODO*/ || bsdf->pluginType() == "thindielectric" /*TODO*/) {
		sstream << "make_glass_bsdf(math, surf, "
				<< ctx.extractMaterialPropertyNumber(bsdf, "ext_ior", 1.000277f) << ", "
				<< ctx.extractMaterialPropertyNumber(bsdf, "int_ior", 1.5046f) << ", "
				<< ctx.extractMaterialPropertyColor(bsdf, "specular_reflectance", 1.0f) << ", "
				<< ctx.extractMaterialPropertyColor(bsdf, "specular_transmittance", 1.0f) << ")";
	} else if (bsdf->pluginType() == "conductor" || bsdf->pluginType() == "roughconductor" /*TODO*/) {
		sstream << "make_conductor_bsdf(math, surf, "
				<< ctx.extractMaterialPropertyNumber(bsdf, "eta", 0.63660f) << ", "
				<< ctx.extractMaterialPropertyNumber(bsdf, "k", 2.7834f) << ", " // TODO: Better defaults?
				<< ctx.extractMaterialPropertyColor(bsdf, "specular_reflectance", 1.0f) << ")";
	} else if (bsdf->pluginType() == "phong" || bsdf->pluginType() == "plastic" /*TODO*/ || bsdf->pluginType() == "roughplastic" /*TODO*/) {
		sstream << "make_phong_bsdf(math, surf, "
				<< ctx.extractMaterialPropertyColor(bsdf, "specular_reflectance", 1.0f) << ", "
				<< ctx.extractMaterialPropertyNumber(bsdf, "exponent", 30) << ")";
	} else if (bsdf->pluginType() == "blendbsdf") {
		if (bsdf->anonymousChildren().size() != 2) {
			error("Invalid blend bsdf");
		} else {
			sstream << extract(bsdf->anonymousChildren()[0], ctx) << ", "
					<< extract(bsdf->anonymousChildren()[1], ctx) << ", "
					<< ctx.extractMaterialPropertyNumber(bsdf, "weight", 0.5f) << ")";
		}
	} else if (bsdf->pluginType() == "mask") {
		if (bsdf->anonymousChildren().size() != 1) {
			error("Invalid mask bsdf");
		} else {
			sstream << "make_mix_bsdf(make_passthrough_bsdf(surf), "
					<< extract(bsdf->anonymousChildren()[0], ctx) << ", "
					<< ctx.extractMaterialPropertyNumber(bsdf, "opacity", 0.5f) << ")";
		}
	} else if (bsdf->pluginType() == "twosided") { /* Ignore */
		if (bsdf->anonymousChildren().size() != 1)
			error("Invalid twosided bsdf");
		else
			sstream << extract(bsdf->anonymousChildren()[0], ctx);
	} else if (bsdf->pluginType() == "null") {
		sstream << "make_black_bsdf()/* Null */";
	} else {
		IG_LOG(L_WARNING) << "Unknown bsdf '" << bsdf->pluginType() << "'" << std::endl;
		sstream << "make_black_bsdf()/* Unknown */";
	}
	return sstream.str();
}
} // namespace IG