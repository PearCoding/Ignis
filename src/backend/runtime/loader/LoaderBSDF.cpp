#include "LoaderBSDF.h"
#include "Loader.h"
#include "Logger.h"
#include "serialization/VectorSerializer.h"

#include "klems/KlemsLoader.h"

namespace IG {

struct BsdfContext {
	LoaderContext& Context;
	LoaderResult& Result;
	std::unordered_map<std::string, std::string> Ignore;
};

enum BsdfFlags {
	BSDF_F_TEX1 = 0x1, // First color parameter is texture
	BSDF_F_TEX2 = 0x2  // Second color parameter is texture
};

enum BsdfType {
	BSDF_DIFFUSE		  = 0x00,
	BSDF_DIFFUSE_TEXTURED = 0x100, // TODO: This is not a good solution...
	BSDF_ORENNAYAR		  = 0x01,
	BSDF_DIELECTRIC		  = 0x02,
	BSDF_ROUGH_DIELECTRIC = 0x03,
	BSDF_THIN_DIELECTRIC  = 0x04,
	BSDF_MIRROR			  = 0x05,
	BSDF_CONDUCTOR		  = 0x06,
	BSDF_ROUGH_CONDUCTOR  = 0x07,
	BSDF_PLASTIC		  = 0x10,
	BSDF_ROUGH_PLASTIC	  = 0x11,
	BSDF_PHONG			  = 0x12,
	BSDF_DISNEY			  = 0x13,
	BSDF_BLEND			  = 0x20,
	BSDF_MASK			  = 0x21,
	BSDF_PASSTROUGH		  = 0x22,
	BSDF_NORMAL_MAP		  = 0x30,
	BSDF_BUMP_MAP		  = 0x31,
	BSDF_KLEMS			  = 0x40,
	BSDF_INVALID		  = 0xFF
};

// Always 3 units
static inline void write_ct(const TextureColorVariant& var, VectorSerializer& serializer)
{
	if (isTexture(var)) {
		serializer.write(extractTexture(var));
		serializer.write((uint32)0);
		serializer.write((uint32)0);
	} else {
		serializer.write(extractColor(var));
	}
}

constexpr float AIR_IOR			   = 1.000277f;
constexpr float GLASS_IOR		   = 1.55f;
constexpr float RUBBER_IOR		   = 1.49f;
constexpr float ETA_DEFAULT		   = 0.63660f;
constexpr float ABSORPTION_DEFAULT = 2.7834f;

static void bsdf_error(const std::string& msg, LoaderResult& result)
{
	IG_LOG(L_ERROR) << msg << std::endl;
	result.Database.BsdfTable.addLookup(BSDF_INVALID, 0, DefaultAlignment);
}

static uint32 setup_microfacet(const std::shared_ptr<Parser::Object>& bsdf, LoaderContext& ctx, VectorSerializer& serializer)
{
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
	return 2;
}

static void bsdf_diffuse(const std::string&, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	const auto albedo = ctx.Context.extractColorTexture(bsdf, "reflectance", Vector3f::Constant(0.5f));
	auto& data		  = ctx.Result.Database.BsdfTable.addLookup(BSDF_DIFFUSE, isTexture(albedo) ? BSDF_F_TEX1 : 0, DefaultAlignment);
	VectorSerializer serializer(data, false);
	write_ct(albedo, serializer);
}

static void bsdf_orennayar(const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	const float alpha = bsdf->property("alpha").getNumber(0.0f);

	if (alpha <= 1e-3f) {
		bsdf_diffuse(name, bsdf, ctx);
		return;
	}

	const auto albedo = ctx.Context.extractColorTexture(bsdf, "reflectance", Vector3f::Constant(0.5f));
	auto& data		  = ctx.Result.Database.BsdfTable.addLookup(BSDF_ORENNAYAR, isTexture(albedo) ? BSDF_F_TEX1 : 0, DefaultAlignment);
	VectorSerializer serializer(data, false);
	write_ct(albedo, serializer);
	serializer.write(alpha);
}

static void bsdf_dielectric(const std::string&, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	auto specular_ref = ctx.Context.extractColorTexture(bsdf, "specular_reflectance");
	auto specular_tra = ctx.Context.extractColorTexture(bsdf, "specular_transmittance");
	float ext_ior	  = ctx.Context.extractIOR(bsdf, "ext_ior", AIR_IOR);
	float int_ior	  = ctx.Context.extractIOR(bsdf, "int_ior", GLASS_IOR);

	uint32 flags = 0;
	if (isTexture(specular_ref))
		flags |= BSDF_F_TEX1;
	if (isTexture(specular_tra))
		flags |= BSDF_F_TEX2;

	auto& data = ctx.Result.Database.BsdfTable.addLookup(BSDF_DIELECTRIC, flags, DefaultAlignment);
	VectorSerializer serializer(data, false);
	write_ct(specular_ref, serializer);
	serializer.write(ext_ior);
	write_ct(specular_tra, serializer);
	serializer.write(int_ior);
}

static void bsdf_thindielectric(const std::string&, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	auto specular_ref = ctx.Context.extractColorTexture(bsdf, "specular_reflectance");
	auto specular_tra = ctx.Context.extractColorTexture(bsdf, "specular_transmittance");
	float ext_ior	  = ctx.Context.extractIOR(bsdf, "ext_ior", AIR_IOR);
	float int_ior	  = ctx.Context.extractIOR(bsdf, "int_ior", GLASS_IOR);

	uint32 flags = 0;
	if (isTexture(specular_ref))
		flags |= BSDF_F_TEX1;
	if (isTexture(specular_tra))
		flags |= BSDF_F_TEX2;

	auto& data = ctx.Result.Database.BsdfTable.addLookup(BSDF_THIN_DIELECTRIC, flags, DefaultAlignment);
	VectorSerializer serializer(data, false);
	write_ct(specular_ref, serializer);
	serializer.write(ext_ior);
	write_ct(specular_tra, serializer);
	serializer.write(int_ior);
}

static void bsdf_mirror(const std::string&, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	auto specular_ref = ctx.Context.extractColorTexture(bsdf, "specular_reflectance");

	uint32 flags = 0;
	if (isTexture(specular_ref))
		flags |= BSDF_F_TEX1;

	auto& data = ctx.Result.Database.BsdfTable.addLookup(BSDF_MIRROR, flags, DefaultAlignment);
	VectorSerializer serializer(data, false);
	write_ct(specular_ref, serializer);
}

static void bsdf_conductor(const std::string&, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	auto specular_ref = ctx.Context.extractColorTexture(bsdf, "specular_reflectance");
	float eta		  = ctx.Context.extractIOR(bsdf, "eta", ETA_DEFAULT);
	float k			  = ctx.Context.extractIOR(bsdf, "k", ABSORPTION_DEFAULT);

	uint32 flags = 0;
	if (isTexture(specular_ref))
		flags |= BSDF_F_TEX1;

	auto& data = ctx.Result.Database.BsdfTable.addLookup(BSDF_CONDUCTOR, flags, DefaultAlignment);
	VectorSerializer serializer(data, false);
	write_ct(specular_ref, serializer);
	serializer.write((uint32)0); //PADDING
	serializer.write(eta);
	serializer.write(k);
}

static void bsdf_rough_conductor(const std::string&, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	auto specular_ref = ctx.Context.extractColorTexture(bsdf, "specular_reflectance");
	float eta		  = ctx.Context.extractIOR(bsdf, "eta", ETA_DEFAULT);
	float k			  = ctx.Context.extractIOR(bsdf, "k", ABSORPTION_DEFAULT);

	uint32 flags = 0;
	if (isTexture(specular_ref))
		flags |= BSDF_F_TEX1;

	auto& data = ctx.Result.Database.BsdfTable.addLookup(BSDF_ROUGH_CONDUCTOR, flags, DefaultAlignment);
	VectorSerializer serializer(data, false);
	write_ct(specular_ref, serializer);
	serializer.write((uint32)0); //PADDING
	serializer.write(eta);
	serializer.write(k);
	setup_microfacet(bsdf, ctx.Context, serializer);
}

static void bsdf_plastic(const std::string&, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	auto specular_ref = ctx.Context.extractColorTexture(bsdf, "specular_reflectance");
	auto diffuse_ref  = ctx.Context.extractColorTexture(bsdf, "diffuse_reflectance", Vector3f::Constant(0.5f));
	float ext_ior	  = ctx.Context.extractIOR(bsdf, "ext_ior", AIR_IOR);
	float int_ior	  = ctx.Context.extractIOR(bsdf, "int_ior", RUBBER_IOR);

	uint32 flags = 0;
	if (isTexture(specular_ref))
		flags |= BSDF_F_TEX1;
	if (isTexture(diffuse_ref))
		flags |= BSDF_F_TEX2;

	auto& data = ctx.Result.Database.BsdfTable.addLookup(BSDF_PLASTIC, flags, DefaultAlignment);
	VectorSerializer serializer(data, false);
	write_ct(specular_ref, serializer);
	serializer.write(ext_ior);
	write_ct(diffuse_ref, serializer);
	serializer.write(int_ior);
}

static void bsdf_rough_plastic(const std::string&, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	auto specular_ref = ctx.Context.extractColorTexture(bsdf, "specular_reflectance");
	auto diffuse_ref  = ctx.Context.extractColorTexture(bsdf, "diffuse_reflectance", Vector3f::Constant(0.5f));
	float ext_ior	  = ctx.Context.extractIOR(bsdf, "ext_ior", AIR_IOR);
	float int_ior	  = ctx.Context.extractIOR(bsdf, "int_ior", RUBBER_IOR);

	uint32 flags = 0;
	if (isTexture(specular_ref))
		flags |= BSDF_F_TEX1;
	if (isTexture(diffuse_ref))
		flags |= BSDF_F_TEX2;

	auto& data = ctx.Result.Database.BsdfTable.addLookup(BSDF_PLASTIC, flags, DefaultAlignment);
	VectorSerializer serializer(data, false);
	write_ct(specular_ref, serializer);
	serializer.write(ext_ior);
	write_ct(diffuse_ref, serializer);
	serializer.write(int_ior);
	setup_microfacet(bsdf, ctx.Context, serializer);
}

static void bsdf_phong(const std::string&, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	auto specular_ref = ctx.Context.extractColorTexture(bsdf, "specular_reflectance");
	float exponent	  = bsdf->property("exponent").getNumber(30);

	uint32 flags = 0;
	if (isTexture(specular_ref))
		flags |= BSDF_F_TEX1;

	auto& data = ctx.Result.Database.BsdfTable.addLookup(BSDF_PHONG, flags, DefaultAlignment);
	VectorSerializer serializer(data, false);
	write_ct(specular_ref, serializer);
	serializer.write(exponent);
}

static void bsdf_disney(const std::string&, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	auto base_color		   = ctx.Context.extractColorTexture(bsdf, "base_color");
	float flatness		   = bsdf->property("flatness").getNumber(0.0f);
	float metallic		   = bsdf->property("metallic").getNumber(0.0f);
	float ior			   = ctx.Context.extractIOR(bsdf, "ior", GLASS_IOR);
	float specular_tint	   = bsdf->property("specular_tint").getNumber(0.0f);
	float roughness		   = bsdf->property("roughness").getNumber(0.5f);
	float anisotropic	   = bsdf->property("anisotropic").getNumber(0.0f);
	float sheen			   = bsdf->property("sheen").getNumber(0.0f);
	float sheen_tint	   = bsdf->property("sheen_tint").getNumber(0.0f);
	float clearcoat		   = bsdf->property("clearcoat").getNumber(0.0f);
	float clearcoat_gloss  = bsdf->property("clearcoat_gloss").getNumber(0.0f);
	float spec_trans	   = bsdf->property("spec_trans").getNumber(0.0f);
	float relative_ior	   = bsdf->property("relative_ior").getNumber(1.1f);
	float scatter_distance = bsdf->property("scatter_distance").getNumber(0.5f);
	float diff_trans	   = bsdf->property("diff_trans").getNumber(0.0f);
	float transmittance	   = bsdf->property("transmittance").getNumber(1.0f);

	uint32 flags = 0;
	if (isTexture(base_color))
		flags |= BSDF_F_TEX1;

	auto& data = ctx.Result.Database.BsdfTable.addLookup(BSDF_DISNEY, flags, DefaultAlignment);
	VectorSerializer serializer(data, false);
	write_ct(base_color, serializer);
	serializer.write(flatness);
	serializer.write(metallic);
	serializer.write(ior);
	serializer.write(specular_tint);
	serializer.write(roughness);
	serializer.write(anisotropic);
	serializer.write(sheen);
	serializer.write(sheen_tint);
	serializer.write(clearcoat);
	serializer.write(clearcoat_gloss);
	serializer.write(spec_trans);
	serializer.write(relative_ior);
	serializer.write(scatter_distance);
	serializer.write(diff_trans);
	serializer.write(transmittance);
}

/*static void bsdf_prep_klems(const std::shared_ptr<Parser::Object>& bsdf, LoaderContext& ctx, std::ostream& os)
{
	const std::filesystem::path filename = ctx.handlePath(bsdf->property("filename").getString());
	const std::string id				 = LoaderContext::makeId(filename);
	const std::string out_file			 = "data/klems_" + filename.stem().generic_string() + ".bin";
	const bool ok						 = KlemsLoader::prepare(filename.generic_string(), out_file);

	os << "    let klems_" << id << " = device.load_klems(\"" << out_file.c_str() << "\");\n";
}

static void bsdf_klems(const std::shared_ptr<Parser::Object>& bsdf, LoaderContext& ctx,  LoaderResult& result)
{
	const std::filesystem::path filename = ctx.handlePath(bsdf->property("filename").getString());
	const std::string id				 = LoaderContext::makeId(filename);

	os << "make_klems_bsdf(math, surf, klems_" << id << ")";
}*/

static void bsdf_blend(const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	const std::string first	 = bsdf->property("first").getString();
	const std::string second = bsdf->property("second").getString();

	if (first.empty() || second.empty()) {
		bsdf_error("Invalid blend bsdf", ctx.Result);
	} else if (first == second) {
		ctx.Ignore[name] = first;
	} else {
		// TODO
		ctx.Ignore[name] = first;
		IG_LOG(L_WARNING) << "Bsdf '" << name << "': Blend currently not implemented. Ignoring effect" << std::endl;
		/* const uint32 firstID  = ctx.Context.Environment.BsdfIDs.at(first);
		const uint32 secondID = ctx.Context.Environment.BsdfIDs.at(second);

		const float weight = bsdf->property("weight").getNumber(0.5f);

		auto& data = result.Database.BsdfTable.addLookup(BSDF_BLEND, DefaultAlignment);
		VectorSerializer serializer(data, false);
		serializer.write(firstID);
		serializer.write(secondID);
		serializer.write(weight); */
	}
}

static void bsdf_mask(const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	const std::string masked = bsdf->property("bsdf").getString();

	if (masked.empty()) {
		bsdf_error("Invalid masked bsdf", ctx.Result);
	} else {
		// TODO
		ctx.Ignore[name] = masked;
		IG_LOG(L_WARNING) << "Bsdf '" << name << "': Mask currently not implemented. Ignoring effect" << std::endl;
		/* const uint32 maskedID = ctx.Context.Environment.BsdfIDs.at(masked);
		const float weight	  = bsdf->property("weight").getNumber(0.5f);

		auto& data = result.Database.BsdfTable.addLookup(BSDF_MASK, DefaultAlignment);
		VectorSerializer serializer(data, false);
		serializer.write(maskedID);
		serializer.write(weight); */
	}
}

static void bsdf_twosided(const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	// Ignore
	const std::string other = bsdf->property("bsdf").getString();

	if (other.empty())
		bsdf_error("Invalid twosided bsdf", ctx.Result);
	else
		ctx.Ignore[name] = other;
}

static void bsdf_passthrough(const std::string&, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	ctx.Result.Database.BsdfTable.addLookup(BSDF_PASSTROUGH, 0, DefaultAlignment);
}

static void bsdf_normalmap(const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	const std::string inner = bsdf->property("bsdf").getString();

	if (inner.empty()) {
		bsdf_error("Invalid normal map bsdf", ctx.Result);
	} else {
		// TODO
		ctx.Ignore[name] = inner;
		IG_LOG(L_WARNING) << "Bsdf '" << name << "': Normalmap currently not implemented. Ignoring effect" << std::endl;
	}
}

static void bsdf_bumpmap(const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx)
{
	const std::string inner = bsdf->property("bsdf").getString();

	if (inner.empty()) {
		bsdf_error("Invalid bump map bsdf", ctx.Result);
	} else {
		// TODO
		ctx.Ignore[name] = inner;
		IG_LOG(L_WARNING) << "Bsdf '" << name << "': Bumpmap currently not implemented. Ignoring effect" << std::endl;
	}
}

using BSDFLoader = void (*)(const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, BsdfContext& ctx);
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
	{ "blendbsdf", bsdf_blend },
	{ "mask", bsdf_mask },
	{ "twosided", bsdf_twosided },
	{ "passthrough", bsdf_passthrough },
	{ "null", bsdf_passthrough },
	{ "bumpmap", bsdf_bumpmap },
	{ "normalmap", bsdf_normalmap },
	{ "", nullptr }
};

bool LoaderBSDF::load(LoaderContext& ctx, LoaderResult& result)
{
	BsdfContext context{ ctx, result, {} };

	for (const auto& pair : ctx.Scene.bsdfs()) {
		const auto bsdf = pair.second;

		bool found = false;
		for (size_t i = 0; _generators[i].Loader; ++i) {
			if (_generators[i].Name == bsdf->pluginType()) {
				ctx.Environment.BsdfIDs[pair.first] = context.Result.Database.BsdfTable.entryCount();
				_generators[i].Loader(pair.first, bsdf, context);
				found = true;
				break;
			}
		}

		if (!found) {
			IG_LOG(L_ERROR) << "Bsdf '" << pair.first << "' has unknown type '" << bsdf->pluginType() << "'" << std::endl;
			result.Database.BsdfTable.addLookup(BSDF_INVALID, 0, DefaultAlignment);
		}
	}

	// Some reordering might happen, take care of it here
	for (const auto& pair : context.Ignore) {
		if (ctx.Environment.BsdfIDs.count(pair.first) == 0)
			IG_LOG(L_ERROR) << "Missing BSDF '" << pair.first << "'" << std::endl;
		else if (ctx.Environment.BsdfIDs.count(pair.second) == 0)
			IG_LOG(L_ERROR) << "Missing BSDF '" << pair.second << "'" << std::endl;
		else {
			ctx.Environment.BsdfIDs[pair.first] = ctx.Environment.BsdfIDs.at(pair.second);
			IG_LOG(L_DEBUG) << "Replacing " << pair.first << " with " << pair.second << std::endl;
		}
	}

	return true;
}
} // namespace IG
