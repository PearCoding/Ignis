#include "LoaderBSDF.h"
#include "Loader.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "serialization/VectorSerializer.h"

#include "klems/KlemsLoader.h"

#include <chrono>

namespace IG {

constexpr float AIR_IOR			   = 1.000277f;
constexpr float GLASS_IOR		   = 1.55f;
constexpr float RUBBER_IOR		   = 1.49f;
constexpr float ETA_DEFAULT		   = 0.63660f;
constexpr float ABSORPTION_DEFAULT = 2.7834f;

/*static void setup_microfacet(const std::shared_ptr<Parser::Object>& bsdf, LoaderContext& ctx, VectorSerializer& serializer)
{
	IG_UNUSED(ctx);
	IG_UNUSED(bsdf);
	
	float alpha_u, alpha_v;
	if (bsdf->property("alpha_u").isValid()) {
		alpha_u = bsdf->property("alpha_u").getNumber(0.1f);
		alpha_v = bsdf->property("alpha_v").getNumber(alpha_u);
	} else {
		alpha_u = bsdf->property("alpha").getNumber(0.1f);
		alpha_v = alpha_u;
	}

	serializer.write(alpha_u);
	serializer.write(alpha_v);
}*/

static void bsdf_diffuse(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
	const auto albedo = ctx.extractColor(bsdf, "reflectance", Vector3f::Constant(0.5f));

	stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = @|_ray, _hit, surf| make_diffuse_bsdf(surf, "
		   << ShaderUtils::inlineColor(albedo) << ");" << std::endl;
}

static void bsdf_orennayar(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
	const float alpha = bsdf->property("alpha").getNumber(0.0f);

	if (alpha <= 1e-3f) {
		bsdf_diffuse(stream, name, bsdf, ctx);
		return;
	}

	const auto albedo = ctx.extractColor(bsdf, "reflectance", Vector3f::Constant(0.5f));

	stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = @|_ray, _hit, surf| make_orennayar_bsdf(surf, "
		   << alpha << ", "
		   << ShaderUtils::inlineColor(albedo) << ");" << std::endl;
}

static void bsdf_dielectric(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
	auto specular_ref = ctx.extractColor(bsdf, "specular_reflectance");
	auto specular_tra = ctx.extractColor(bsdf, "specular_transmittance");
	float ext_ior	  = ctx.extractIOR(bsdf, "ext_ior", AIR_IOR);
	float int_ior	  = ctx.extractIOR(bsdf, "int_ior", GLASS_IOR);

	stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = @|_ray, _hit, surf| make_glass_bsdf(surf, "
		   << ext_ior << ", "
		   << int_ior << ", "
		   << ShaderUtils::inlineColor(specular_ref) << ", "
		   << ShaderUtils::inlineColor(specular_tra) << ");" << std::endl;
}

static void bsdf_thindielectric(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
	auto specular_ref = ctx.extractColor(bsdf, "specular_reflectance");
	auto specular_tra = ctx.extractColor(bsdf, "specular_transmittance");
	float ext_ior	  = ctx.extractIOR(bsdf, "ext_ior", AIR_IOR);
	float int_ior	  = ctx.extractIOR(bsdf, "int_ior", GLASS_IOR);

	stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = @|_ray, _hit, surf| make_thin_glass_bsdf(surf, "
		   << ext_ior << ", "
		   << int_ior << ", "
		   << ShaderUtils::inlineColor(specular_ref) << ", "
		   << ShaderUtils::inlineColor(specular_tra) << ");" << std::endl;
}

static void bsdf_mirror(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
	auto specular_ref = ctx.extractColor(bsdf, "specular_reflectance");

	stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = @|_ray, _hit, surf| make_mirror_bsdf(surf, "
		   << ShaderUtils::inlineColor(specular_ref) << ");" << std::endl;
}

static void bsdf_conductor(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
	auto specular_ref = ctx.extractColor(bsdf, "specular_reflectance");
	float eta		  = ctx.extractIOR(bsdf, "eta", ETA_DEFAULT);
	float k			  = ctx.extractIOR(bsdf, "k", ABSORPTION_DEFAULT);

	stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = @|_ray, _hit, surf| make_thin_glass_bsdf(surf, "
		   << eta << ", "
		   << k << ", "
		   << ShaderUtils::inlineColor(specular_ref) << ");" << std::endl;
}

static void bsdf_rough_conductor(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
	auto specular_ref = ctx.extractColor(bsdf, "specular_reflectance");
	float eta		  = ctx.extractIOR(bsdf, "eta", ETA_DEFAULT);
	float k			  = ctx.extractIOR(bsdf, "k", ABSORPTION_DEFAULT);

	// TODO
	stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = @|_ray, _hit, surf| make_thin_glass_bsdf(surf, "
		   << eta << ", "
		   << k << ", "
		   << ShaderUtils::inlineColor(specular_ref) << ");" << std::endl;
}

static void bsdf_plastic(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
	auto specular_ref = ctx.extractColor(bsdf, "specular_reflectance");
	auto diffuse_ref  = ctx.extractColor(bsdf, "diffuse_reflectance", Vector3f::Constant(0.5f));
	float ext_ior	  = ctx.extractIOR(bsdf, "ext_ior", AIR_IOR);
	float int_ior	  = ctx.extractIOR(bsdf, "int_ior", RUBBER_IOR);

	stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = @|_ray, _hit, surf| make_plastic_bsdf(surf, "
		   << ext_ior << ", "
		   << int_ior << ", "
		   << ShaderUtils::inlineColor(diffuse_ref) << ", "
		   << "make_mirror_bsdf(surf, " << ShaderUtils::inlineColor(specular_ref) << "));" << std::endl;
}

static void bsdf_rough_plastic(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
	auto specular_ref = ctx.extractColor(bsdf, "specular_reflectance");
	auto diffuse_ref  = ctx.extractColor(bsdf, "diffuse_reflectance", Vector3f::Constant(0.5f));
	float ext_ior	  = ctx.extractIOR(bsdf, "ext_ior", AIR_IOR);
	float int_ior	  = ctx.extractIOR(bsdf, "int_ior", RUBBER_IOR);

	// TODO
	stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = @|_ray, _hit, surf| make_plastic_bsdf(surf, "
		   << ext_ior << ", "
		   << int_ior << ", "
		   << ShaderUtils::inlineColor(diffuse_ref) << ", "
		   << "make_mirror_bsdf(surf, " << ShaderUtils::inlineColor(specular_ref) << "));" << std::endl;
}

static void bsdf_phong(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
	auto specular_ref = ctx.extractColor(bsdf, "specular_reflectance");
	float exponent	  = bsdf->property("exponent").getNumber(30);

	stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = @|_ray, _hit, surf| make_phong_bsdf(surf, "
		   << ShaderUtils::inlineColor(specular_ref) << ", "
		   << exponent << ");" << std::endl;
}

static void bsdf_disney(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
	auto base_color = ctx.extractColor(bsdf, "base_color");
	float flatness	= bsdf->property("flatness").getNumber(0.0f);
	float metallic	= bsdf->property("metallic").getNumber(1.0f);
	// float ior			   = ctx.extractIOR(bsdf, "ior", GLASS_IOR);
	float ior			   = 1.0;
	float specular_tint	   = bsdf->property("specular_tint").getNumber(0.0f);
	float roughness		   = bsdf->property("roughness").getNumber(0.6f);
	float anisotropic	   = bsdf->property("anisotropic").getNumber(0.0f);
	float sheen			   = bsdf->property("sheen").getNumber(1.0f);
	float sheen_tint	   = bsdf->property("sheen_tint").getNumber(0.0f);
	float clearcoat		   = bsdf->property("clearcoat").getNumber(1.0f);
	float clearcoat_gloss  = bsdf->property("clearcoat_gloss").getNumber(1.0f);
	float spec_trans	   = bsdf->property("spec_trans").getNumber(0.0f);
	float relative_ior	   = bsdf->property("relative_ior").getNumber(1.0f);
	float scatter_distance = bsdf->property("scatter_distance").getNumber(0.5f);
	float diff_trans	   = bsdf->property("diff_trans").getNumber(0.0f);
	float transmittance	   = bsdf->property("transmittance").getNumber(1.0f);

	stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = @|_ray, _hit, surf| make_disney_bsdf(surf, "
		   << ShaderUtils::inlineColor(base_color) << ", "
		   << flatness << ", "
		   << metallic << ", "
		   << ior << ", "
		   << specular_tint << ", "
		   << roughness << ", "
		   << anisotropic << ", "
		   << sheen << ", "
		   << sheen_tint << ", "
		   << clearcoat << ", "
		   << clearcoat_gloss << ", "
		   << spec_trans << ", "
		   << relative_ior << ", "
		   << scatter_distance << ", "
		   << diff_trans << ", "
		   << transmittance << ");" << std::endl;
}

static void bsdf_twosided(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
	// Ignore
	const std::string other = bsdf->property("bsdf").getString();
	stream << LoaderBSDF::generate(other, ctx);
}

static void bsdf_passthrough(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>&, const LoaderContext& ctx)
{
	stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = @|_ray, _hit, surf| make_passthrough_bsdf(surf);" << std::endl;
}

using BSDFLoader = void (*)(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx);
static struct {
	const char* Name;
	BSDFLoader Loader;
} _generators[] = {
	{ "diffuse", bsdf_diffuse },
	{ "roughdiffuse", bsdf_orennayar },
	{ "glass", bsdf_dielectric },
	{ "dielectric", bsdf_dielectric },
	{ "roughdielectric", bsdf_dielectric }, // TODO
	{ "thindielectric", bsdf_thindielectric },
	{ "mirror", bsdf_mirror }, // Specialized conductor
	{ "conductor", bsdf_conductor },
	{ "roughconductor", bsdf_rough_conductor },
	{ "phong", bsdf_phong },
	{ "disney", bsdf_disney },
	{ "plastic", bsdf_plastic },
	{ "roughplastic", bsdf_rough_plastic },
	/*{ "klems", bsdf_klems },*/
	//{ "blendbsdf", bsdf_blend },
	//{ "mask", bsdf_mask },
	{ "twosided", bsdf_twosided },
	{ "passthrough", bsdf_passthrough },
	{ "null", bsdf_passthrough },
	//{ "bumpmap", bsdf_bumpmap },
	//{ "normalmap", bsdf_normalmap },
	{ "", nullptr }
};

std::string LoaderBSDF::generate(const std::string& name, const LoaderContext& ctx)
{
	std::stringstream stream;
	const auto bsdf = ctx.Scene.bsdf(name);

	bool found = false;
	for (size_t i = 0; _generators[i].Loader; ++i) {
		if (_generators[i].Name == bsdf->pluginType()) {
			_generators[i].Loader(stream, name, bsdf, ctx);
			found = true;
			break;
		}
	}
	
	if (!found)
		IG_LOG(L_ERROR) << "No bsdf type '" << bsdf->pluginType() << "' available" << std::endl;

	return stream.str();
}
} // namespace IG
