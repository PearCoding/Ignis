#include "LoaderBSDF.h"
#include "Loader.h"
#include "LoaderUtils.h"
#include "Logger.h"
#include "ShadingTree.h"

#include "measured/KlemsLoader.h"
#include "measured/TensorTreeLoader.h"

#include "measured/djmeasured.h"

#include <chrono>

namespace IG {

const static std::unordered_map<std::string_view, float> Dielectrics = {
    { "vacuum", 1.0f },
    { "bk7", 1.5046f },
    { "glass", 1.5046f },
    { "helium", 1.00004f },
    { "hydrogen", 1.00013f },
    { "air", 1.000277f },
    { "water", 1.333f },
    { "ethanol", 1.361f },
    { "diamond", 2.419f },
    { "polypropylene", 1.49f }
};

const static float DefaultDielectricInterior = Dielectrics.at("bk7");
const static float DefaultDielectricExterior = Dielectrics.at("vacuum");
const static float DefaultPlastic            = Dielectrics.at("polypropylene");

std::optional<float> lookupDielectric(const std::string& prop, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf)
{
    if (bsdf->property(prop).type() == Parser::PT_STRING) {
        const std::string material = to_lowercase(bsdf->property(prop).getString());
        if (Dielectrics.count(material) > 0)
            return Dielectrics.at(material);
        else
            IG_LOG(L_ERROR) << "Bsdf '" << name << "' has unknown " << prop << " name '" << material << "' given" << std::endl;
    }
    return {};
}

struct ConductorSpec {
    Vector3f Eta;
    Vector3f Kappa;
};

// Materials from https://chris.hindefjord.se/resources/rgb-ior-metals/
const static std::unordered_map<std::string_view, ConductorSpec> Conductors = {
    { "aluminum", { { 1.34560f, 0.96521f, 0.61722f }, { 7.47460f, 6.39950f, 5.30310f } } },
    { "brass", { { 0.44400f, 0.52700f, 1.09400f }, { 3.69500f, 2.76500f, 1.82900f } } },
    { "copper", { { 0.27105f, 0.67693f, 1.31640f }, { 3.60920f, 2.62480f, 2.29210f } } },
    { "gold", { { 0.18299f, 0.42108f, 1.37340f }, { 3.4242f, 2.34590f, 1.77040f } } },
    { "iron", { { 2.91140f, 2.94970f, 2.58450f }, { 3.08930f, 2.93180f, 2.76700f } } },
    { "lead", { { 1.91000f, 1.83000f, 1.44000f }, { 3.51000f, 3.40000f, 3.18000f } } },
    { "mercury", { { 2.07330f, 1.55230f, 1.06060f }, { 5.33830f, 4.65100f, 3.86280f } } },
    { "platinum", { { 2.37570f, 2.08470f, 1.84530f }, { 4.26550f, 3.71530f, 3.13650f } } },
    { "silver", { { 0.15943f, 0.14512f, 0.13547f }, { 3.92910f, 3.19000f, 2.38080f } } },
    { "titanium", { { 2.74070f, 2.54180f, 2.26700f }, { 3.81430f, 3.43450f, 3.03850f } } },
    { "none", { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } } }
};
const static auto& DefaultConductor = Conductors.at("none");

std::optional<ConductorSpec> lookupConductor(const std::string& name, const std::shared_ptr<Parser::Object>& bsdf)
{
    if (bsdf->property("material").type() == Parser::PT_STRING) {
        const std::string material = to_lowercase(bsdf->property("material").getString());
        if (Conductors.count(material) > 0)
            return Conductors.at(material);
        else
            IG_LOG(L_ERROR) << "Bsdf '" << name << "' has unknown material name '" << material << "' given" << std::endl;
    }
    return {};
}

static void setup_microfacet(const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.addNumber("alpha_u", *bsdf, 0.1f, false);
    tree.addNumber("alpha_v", *bsdf, 0.1f, false);
    tree.addNumber("alpha", *bsdf, 0.1f);
}

static std::string inline_microfacet(const std::string& name, ShadingTree& tree, const std::shared_ptr<Parser::Object>& bsdf)
{
    std::string distribution = "microfacet::make_vndf_ggx_distribution";
    if (bsdf->property("distribution").type() == Parser::PT_STRING) {
        std::string type = bsdf->property("distribution").getString();
        if (type == "ggx") {
            distribution = "microfacet::make_ggx_distribution";
        } else if (type == "beckmann") {
            distribution = "microfacet::make_beckmann_distribution";
        }
    }

    std::stringstream stream;
    if (tree.hasParameter("alpha_u")) {
        stream << "  let md_" << LoaderUtils::escapeIdentifier(name) << " = @|surf : SurfaceElement| " << distribution << "(surf.local, "
               << tree.getInline("alpha_u") << ", "
               << tree.getInline("alpha_v") << ");" << std::endl;
    } else {
        stream << "  let md_" << LoaderUtils::escapeIdentifier(name) << " = @|surf : SurfaceElement| " << distribution << "(surf.local, "
               << tree.getInline("alpha") << ", "
               << tree.getInline("alpha") << ");" << std::endl;
    }
    return stream.str();
}

static void bsdf_djmeasured(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree) 
{

    std::string bsdf_name = bsdf->property("filename").getString();
    std::string bsdf_path = "../brdf/" + bsdf_name + "_rgb.bsdf";
    std::string out_path = "../brdf/" + bsdf_name + ".bin";
    std::string buffer_name = "buffer_" + bsdf_name;

    BRDFData* data = load_brdf_data(bsdf_path);
    write_brdf_data(data, out_path);

    tree.beginClosure();

    stream << tree.pullHeader()
           << "  let " << buffer_name << " : DeviceBuffer = device.load_buffer(\"" << out_path << "\");"
           << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_djmeasured_bsdf(surf, "
           << buffer_name << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_diffuse(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("reflectance", *bsdf, Vector3f::Constant(0.5f));

    stream << tree.pullHeader()
           << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_diffuse_bsdf(surf, "
           << tree.getInline("reflectance") << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_orennayar(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addNumber("alpha", *bsdf, 0.0f);
    tree.addColor("reflectance", *bsdf, Vector3f::Constant(0.5f));

    stream << tree.pullHeader()
           << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_orennayar_bsdf(surf, "
           << tree.getInline("alpha") << ", "
           << tree.getInline("reflectance") << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_dielectric(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addColor("specular_transmittance", *bsdf, Vector3f::Ones());
    tree.addNumber("ext_ior", *bsdf, DefaultDielectricExterior);
    tree.addNumber("int_ior", *bsdf, DefaultDielectricInterior);

    const auto ext_spec = lookupDielectric("ext_ior_material", name, bsdf);
    const auto int_spec = lookupDielectric("int_ior_material", name, bsdf);

    bool thin = bsdf->property("thin").getBool(bsdf->pluginType() == "thindielectric");

    stream << tree.pullHeader()
           << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| "
           << (thin ? "make_thin_glass_bsdf" : "make_glass_bsdf") << "(surf, "
           << (ext_spec.has_value() ? std::to_string(ext_spec.value()) : tree.getInline("ext_ior")) << ", "
           << (int_spec.has_value() ? std::to_string(int_spec.value()) : tree.getInline("int_ior")) << ", "
           << tree.getInline("specular_reflectance") << ", "
           << tree.getInline("specular_transmittance") << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_rough_dielectric(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addColor("specular_transmittance", *bsdf, Vector3f::Ones());
    tree.addNumber("ext_ior", *bsdf, DefaultDielectricExterior);
    tree.addNumber("int_ior", *bsdf, DefaultDielectricInterior);

    const auto ext_spec = lookupDielectric("ext_ior_material", name, bsdf);
    const auto int_spec = lookupDielectric("int_ior_material", name, bsdf);

    setup_microfacet(bsdf, tree);
    stream << tree.pullHeader()
           << inline_microfacet(name, tree, bsdf)
           << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_rough_glass_bsdf(surf, "
           << (ext_spec.has_value() ? std::to_string(ext_spec.value()) : tree.getInline("ext_ior")) << ", "
           << (int_spec.has_value() ? std::to_string(int_spec.value()) : tree.getInline("int_ior")) << ", "
           << tree.getInline("specular_reflectance") << ", "
           << tree.getInline("specular_transmittance") << ", "
           << "md_" << LoaderUtils::escapeIdentifier(name) << "(surf));" << std::endl;

    tree.endClosure();
}

static void bsdf_mirror(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());

    stream << tree.pullHeader()
           << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_mirror_bsdf(surf, "
           << tree.getInline("specular_reflectance") << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_conductor(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{

    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addColor("eta", *bsdf, DefaultConductor.Eta);
    tree.addColor("k", *bsdf, DefaultConductor.Kappa);

    const auto spec = lookupConductor(name, bsdf);

    stream << tree.pullHeader()
           << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_conductor_bsdf(surf, "
           << (spec.has_value() ? LoaderUtils::inlineColor(spec.value().Eta) : tree.getInline("eta")) << ", "
           << (spec.has_value() ? LoaderUtils::inlineColor(spec.value().Kappa) : tree.getInline("k")) << ", "
           << tree.getInline("specular_reflectance") << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_rough_conductor(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addColor("eta", *bsdf, DefaultConductor.Eta);
    tree.addColor("k", *bsdf, DefaultConductor.Kappa);

    const auto spec = lookupConductor(name, bsdf);

    setup_microfacet(bsdf, tree);
    stream << tree.pullHeader()
           << inline_microfacet(name, tree, bsdf)
           << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_rough_conductor_bsdf(surf, "
           << (spec.has_value() ? LoaderUtils::inlineColor(spec.value().Eta) : tree.getInline("eta")) << ", "
           << (spec.has_value() ? LoaderUtils::inlineColor(spec.value().Kappa) : tree.getInline("k")) << ", "
           << tree.getInline("specular_reflectance") << ", "
           << "md_" << LoaderUtils::escapeIdentifier(name) << "(surf));" << std::endl;

    tree.endClosure();
}

static void bsdf_plastic(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addColor("diffuse_reflectance", *bsdf, Vector3f::Constant(0.5f));
    tree.addNumber("ext_ior", *bsdf, DefaultDielectricExterior);
    tree.addNumber("int_ior", *bsdf, DefaultPlastic);

    const auto ext_spec = lookupDielectric("ext_ior_material", name, bsdf);
    const auto int_spec = lookupDielectric("int_ior_material", name, bsdf);

    stream << tree.pullHeader()
           << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_plastic_bsdf(surf, "
           << (ext_spec.has_value() ? std::to_string(ext_spec.value()) : tree.getInline("ext_ior")) << ", "
           << (int_spec.has_value() ? std::to_string(int_spec.value()) : tree.getInline("int_ior")) << ", "
           << tree.getInline("diffuse_reflectance") << ", "
           << "make_mirror_bsdf(surf, " << tree.getInline("specular_reflectance") << "));" << std::endl;

    tree.endClosure();
}

static void bsdf_rough_plastic(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addColor("diffuse_reflectance", *bsdf, Vector3f::Constant(0.5f));
    tree.addNumber("ext_ior", *bsdf, DefaultDielectricExterior);
    tree.addNumber("int_ior", *bsdf, DefaultPlastic);

    const auto ext_spec = lookupDielectric("ext_ior_material", name, bsdf);
    const auto int_spec = lookupDielectric("int_ior_material", name, bsdf);

    setup_microfacet(bsdf, tree);
    stream << tree.pullHeader()
           << inline_microfacet(name, tree, bsdf)
           << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_plastic_bsdf(surf, "
           << (ext_spec.has_value() ? std::to_string(ext_spec.value()) : tree.getInline("ext_ior")) << ", "
           << (int_spec.has_value() ? std::to_string(int_spec.value()) : tree.getInline("int_ior")) << ", "
           << tree.getInline("diffuse_reflectance") << ", "
           << "make_rough_conductor_bsdf(surf, color_builtins::black, color_builtins::white, "
           << tree.getInline("specular_reflectance") << ", "
           << "md_" << LoaderUtils::escapeIdentifier(name) << "(surf)));" << std::endl;

    tree.endClosure();
}

static void bsdf_phong(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addNumber("exponent", *bsdf, 30);

    stream << tree.pullHeader()
           << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_phong_bsdf(surf, "
           << tree.getInline("specular_reflectance") << ", "
           << tree.getInline("exponent") << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_principled(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("base_color", *bsdf, Vector3f::Constant(0.8f));
    tree.addNumber("ior", *bsdf, DefaultDielectricInterior);
    tree.addNumber("diffuse_transmission", *bsdf, 0);
    tree.addNumber("specular_transmission", *bsdf, 0);
    tree.addNumber("specular_tint", *bsdf, 0);
    tree.addNumber("roughness", *bsdf, 0.5f);
    tree.addNumber("anisotropic", *bsdf, 0);
    tree.addNumber("flatness", *bsdf, 0);
    tree.addNumber("metallic", *bsdf, 0);
    tree.addNumber("sheen", *bsdf, 0);
    tree.addNumber("sheen_tint", *bsdf, 0);
    tree.addNumber("clearcoat", *bsdf, 0);
    tree.addNumber("clearcoat_gloss", *bsdf, 0);
    tree.addNumber("clearcoat_roughness", *bsdf, 0.1f);

    const auto ior_spec = lookupDielectric("ior_material", name, bsdf);

    bool is_thin            = bsdf->property("thin").getBool(false);
    bool clearcoat_top_only = bsdf->property("clearcoat_top_only").getBool(true);

    stream << tree.pullHeader()
           << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_principled_bsdf(surf, "
           << tree.getInline("base_color") << ", "
           << (ior_spec.has_value() ? std::to_string(ior_spec.value()) : tree.getInline("ior")) << ", "
           << tree.getInline("diffuse_transmission") << ", "
           << tree.getInline("specular_transmission") << ", "
           << tree.getInline("specular_tint") << ", "
           << tree.getInline("roughness") << ", "
           << tree.getInline("anisotropic") << ", "
           << tree.getInline("flatness") << ", "
           << tree.getInline("metallic") << ", "
           << tree.getInline("sheen") << ", "
           << tree.getInline("sheen_tint") << ", "
           << tree.getInline("clearcoat") << ", "
           << tree.getInline("clearcoat_gloss") << ", "
           << tree.getInline("clearcoat_roughness") << ", "
           << (is_thin ? "true" : "false") << ", "
           << (clearcoat_top_only ? "true" : "false") << ");" << std::endl;

    tree.endClosure();
}

using KlemsExportedData = std::pair<std::string, KlemsSpecification>;
static KlemsExportedData setup_klems(const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, LoaderContext& ctx)
{
    auto filename = ctx.handlePath(bsdf->property("filename").getString(), *bsdf);

    const std::string exported_id = "_klems_" + filename.u8string();

    const auto data = ctx.ExportedData.find(exported_id);
    if (data != ctx.ExportedData.end())
        return std::any_cast<KlemsExportedData>(data->second);

    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/klems_" + LoaderUtils::escapeIdentifier(name) + ".bin";

    KlemsSpecification spec{};
    if (!KlemsLoader::prepare(filename, path, spec))
        ctx.signalError();

    const KlemsExportedData res   = { path, spec };
    ctx.ExportedData[exported_id] = res;
    return res;
}

static inline std::string dump_klems_specification(const KlemsComponentSpecification& spec)
{
    std::stringstream stream;
    stream << "KlemsComponentSpecification{ total=" << spec.total
           << ", theta_count=[" << spec.theta_count.first << ", " << spec.theta_count.second << "]"
           << ", entry_count=[" << spec.entry_count.first << ", " << spec.entry_count.second << "]"
           << "}";
    return stream.str();
}

static inline std::string dump_klems_specification(const KlemsSpecification& spec)
{
    std::stringstream stream;
    stream << "KlemsSpecification{"
           << "  front_reflection=" << dump_klems_specification(spec.front_reflection)
           << ", back_reflection=" << dump_klems_specification(spec.back_reflection)
           << ", front_transmission=" << dump_klems_specification(spec.front_transmission)
           << ", back_transmission=" << dump_klems_specification(spec.back_transmission)
           << "}";
    return stream.str();
}

static void bsdf_klems(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("base_color", *bsdf, Vector3f::Ones());

    const Vector3f upVector = bsdf->property("up").getVector3(Vector3f::UnitZ()).normalized();

    const std::string id = LoaderUtils::escapeIdentifier(name);
    const auto data      = setup_klems(name, bsdf, tree.context());

    const std::string buffer_path = std::get<0>(data);
    const KlemsSpecification spec = std::get<1>(data);

    stream << tree.pullHeader()
           << "  let klems_" << id << " = make_klems_model(device.load_buffer(\"" << buffer_path << "\"), "
           << dump_klems_specification(spec) << ");" << std::endl
           << "  let bsdf_" << id << " : BSDFShader = @|_ray, _hit, surf| make_klems_bsdf(surf, "
           << tree.getInline("base_color") << ", "
           << LoaderUtils::inlineVector(upVector) << ", "
           << "klems_" << id << ");" << std::endl;

    tree.endClosure();
}

using TTExportedData = std::pair<std::string, TensorTreeSpecification>;
static TTExportedData setup_tensortree(const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, LoaderContext& ctx)
{
    auto filename = ctx.handlePath(bsdf->property("filename").getString(), *bsdf);

    const std::string exported_id = "_tt_" + filename.u8string();

    const auto data = ctx.ExportedData.find(exported_id);
    if (data != ctx.ExportedData.end())
        return std::any_cast<TTExportedData>(data->second);

    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/tt_" + LoaderUtils::escapeIdentifier(name) + ".bin";

    TensorTreeSpecification spec{};
    if (!TensorTreeLoader::prepare(filename, path, spec))
        ctx.signalError();

    const TTExportedData res      = { path, spec };
    ctx.ExportedData[exported_id] = res;
    return res;
}

static inline std::string dump_tt_specification(const TensorTreeSpecification& parent, const TensorTreeComponentSpecification& spec)
{
    std::stringstream stream;
    stream << "TensorTreeComponentSpecification{ ndim=" << parent.ndim
           << ", node_count=" << spec.node_count
           << ", value_count=" << spec.value_count
           << ", total=" << spec.total
           << ", root_is_leaf=" << (spec.root_is_leaf ? "true" : "false")
           << "}";
    return stream.str();
}

static inline std::string dump_tt_specification(const TensorTreeSpecification& spec)
{
    std::stringstream stream;
    stream << "TensorTreeSpecification{ ndim=" << spec.ndim
           << ", front_reflection=" << dump_tt_specification(spec, spec.front_reflection)
           << ", back_reflection=" << dump_tt_specification(spec, spec.back_reflection)
           << ", front_transmission=" << dump_tt_specification(spec, spec.front_transmission)
           << ", back_transmission=" << dump_tt_specification(spec, spec.back_transmission)
           << "}";
    return stream.str();
}

static void bsdf_tensortree(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("base_color", *bsdf, Vector3f::Ones());

    const Vector3f upVector = bsdf->property("up").getVector3(Vector3f::UnitZ()).normalized();

    const std::string id = LoaderUtils::escapeIdentifier(name);
    const auto data      = setup_tensortree(name, bsdf, tree.context());

    const std::string buffer_path      = std::get<0>(data);
    const TensorTreeSpecification spec = std::get<1>(data);

    stream << tree.pullHeader()
           << "  let tt_" << id << " = make_tensortree_model(device.request_debug_output(), device.load_buffer(\"" << buffer_path << "\"), "
           << dump_tt_specification(spec) << ");" << std::endl
           << "  let bsdf_" << id << " : BSDFShader = @|_ray, _hit, surf| make_tensortree_bsdf(surf, "
           << tree.getInline("base_color") << ", "
           << LoaderUtils::inlineVector(upVector) << ", "
           << "tt_" << id << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_twosided(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    // Ignore
    const std::string other = bsdf->property("bsdf").getString();
    stream << LoaderBSDF::generate(other, tree);
    stream << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " = bsdf_" << LoaderUtils::escapeIdentifier(other) << ";" << std::endl;
}

static void bsdf_passthrough(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>&, ShadingTree& tree)
{
    IG_UNUSED(tree);
    stream << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_passthrough_bsdf(surf);" << std::endl;
}

static void bsdf_blend(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    const std::string first  = bsdf->property("first").getString();
    const std::string second = bsdf->property("second").getString();

    if (first.empty() || second.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdfs given" << std::endl;
        stream << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_error_bsdf(surf);" << std::endl;
    } else if (first == second) {
        // Ignore it
        stream << LoaderBSDF::generate(first, tree);
        stream << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " = bsdf_" << LoaderUtils::escapeIdentifier(first) << ";" << std::endl;
    } else {
        tree.beginClosure();
        tree.addNumber("weight", *bsdf, 0.5f);

        stream << LoaderBSDF::generate(first, tree);
        stream << LoaderBSDF::generate(second, tree);

        stream << tree.pullHeader()
               << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_mix_bsdf("
               << "bsdf_" << LoaderUtils::escapeIdentifier(first) << "(ray, hit, surf), "
               << "bsdf_" << LoaderUtils::escapeIdentifier(second) << "(ray, hit, surf), "
               << tree.getInline("weight") << ");" << std::endl;

        tree.endClosure();
    }
}

static void bsdf_mask(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    const std::string masked = bsdf->property("bsdf").getString();
    const bool inverted      = bsdf->property("inverted").getBool();

    if (masked.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdf given" << std::endl;
        stream << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_error_bsdf(surf);" << std::endl;
    } else {
        tree.beginClosure();
        tree.addNumber("weight", *bsdf, 0.5f);

        stream << LoaderBSDF::generate(masked, tree);

        stream << tree.pullHeader()
               << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_mix_bsdf(";
        if (inverted) {
            stream
                << "make_passthrough_bsdf(surf), "
                << "bsdf_" << LoaderUtils::escapeIdentifier(masked) << "(ray, hit, surf), ";
        } else {
            stream
                << "bsdf_" << LoaderUtils::escapeIdentifier(masked) << "(ray, hit, surf), "
                << "make_passthrough_bsdf(surf), ";
        }
        stream << tree.getInline("weight") << ");" << std::endl;

        tree.endClosure();
    }
}

static void bsdf_cutoff(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    const std::string masked = bsdf->property("bsdf").getString();
    const bool inverted      = bsdf->property("inverted").getBool();

    if (masked.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdf given" << std::endl;
        stream << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_error_bsdf(surf);" << std::endl;
    } else {
        tree.beginClosure();
        tree.addNumber("weight", *bsdf, 0.5f); // Only useful with textures or other patterns
        tree.addNumber("cutoff", *bsdf, 0.5f);

        stream << LoaderBSDF::generate(masked, tree);

        stream << tree.pullHeader()
               << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_mix_bsdf(";

        if (inverted) {
            stream
                << "make_passthrough_bsdf(surf), "
                << "bsdf_" << LoaderUtils::escapeIdentifier(masked) << "(ray, hit, surf), ";
        } else {
            stream
                << "bsdf_" << LoaderUtils::escapeIdentifier(masked) << "(ray, hit, surf), "
                << "make_passthrough_bsdf(surf), ";
        }

        stream << "if " << tree.getInline("weight") << " < " << tree.getInline("cutoff") << " { 0:f32 } else { 1:f32 } );" << std::endl;

        tree.endClosure();
    }
}

static void bsdf_normalmap(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    const std::string inner = bsdf->property("bsdf").getString();
    tree.beginClosure();
    tree.addColor("map", *bsdf, Vector3f::Constant(1.0f));
    tree.addNumber("strength", *bsdf, 1.0f);

    if (inner.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdf given" << std::endl;
        stream << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_error_bsdf(surf);" << std::endl;
    } else {
        stream << LoaderBSDF::generate(inner, tree);

        stream << tree.pullHeader()
               << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_normalmap(surf, @|surf2| -> Bsdf { "
               << " bsdf_" << LoaderUtils::escapeIdentifier(inner) << "(ray, hit, surf2) }, "
               << tree.getInline("map") << ","
               << tree.getInline("strength") << ");" << std::endl;
    }
    tree.endClosure();
}

static void bsdf_bumpmap(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    const std::string inner = bsdf->property("bsdf").getString();
    tree.beginClosure();
    tree.addTexture("map", *bsdf); // Better use some node system with explicit gradients...
    tree.addNumber("strength", *bsdf, 1.0f);

    if (inner.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdf given" << std::endl;
        stream << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_error_bsdf(surf);" << std::endl;
    } else {
        stream << LoaderBSDF::generate(inner, tree);

        stream << tree.pullHeader()
               << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_bumpmap(surf, "
               << "@|surf2| -> Bsdf {  bsdf_" << LoaderUtils::escapeIdentifier(inner) << "(ray, hit, surf2) }, "
               << "texture_dx(" << tree.getInline("map") << ", surf.tex_coords).r, "
               << "texture_dy(" << tree.getInline("map") << ", surf.tex_coords).r, "
               << tree.getInline("strength") << ");" << std::endl;
    }
    tree.endClosure();
}

static void bsdf_doublesided(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    const std::string inner = bsdf->property("bsdf").getString();
    tree.beginClosure();

    if (inner.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdf given" << std::endl;
        stream << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_error_bsdf(surf);" << std::endl;
    } else {
        stream << LoaderBSDF::generate(inner, tree);

        stream << tree.pullHeader()
               << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_doublesided_bsdf(surf, "
               << "@|surf2| -> Bsdf {  bsdf_" << LoaderUtils::escapeIdentifier(inner) << "(ray, hit, surf2) });" << std::endl;
    }
    tree.endClosure();
}

using BSDFLoader = void (*)(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree);
static const struct {
    const char* Name;
    BSDFLoader Loader;
} _generators[] = {
    { "djmeasured", bsdf_djmeasured },
    { "diffuse", bsdf_diffuse },
    { "roughdiffuse", bsdf_orennayar },
    { "glass", bsdf_dielectric },
    { "dielectric", bsdf_dielectric },
    { "roughdielectric", bsdf_rough_dielectric },
    { "thindielectric", bsdf_dielectric },
    { "mirror", bsdf_mirror }, // Specialized conductor
    { "conductor", bsdf_conductor },
    { "roughconductor", bsdf_rough_conductor },
    { "phong", bsdf_phong },
    { "disney", bsdf_principled },
    { "principled", bsdf_principled },
    { "plastic", bsdf_plastic },
    { "roughplastic", bsdf_rough_plastic },
    { "klems", bsdf_klems },
    { "tensortree", bsdf_tensortree },
    { "blend", bsdf_blend },
    { "mask", bsdf_mask },
    { "cutoff", bsdf_cutoff },
    { "twosided", bsdf_twosided },
    { "passthrough", bsdf_passthrough },
    { "null", bsdf_passthrough },
    { "bumpmap", bsdf_bumpmap },
    { "normalmap", bsdf_normalmap },
    { "doublesided", bsdf_doublesided },
    { "", nullptr }
};

std::string LoaderBSDF::generate(const std::string& name, ShadingTree& tree)
{
    std::stringstream stream;
    const auto bsdf = tree.context().Scene.bsdf(name);

    bool error = false;

    if (!bsdf) {
        IG_LOG(L_ERROR) << "Unknown bsdf '" << name << "'" << std::endl;
        error = true;
    } else {

        bool found = false;
        for (size_t i = 0; _generators[i].Loader; ++i) {
            if (_generators[i].Name == bsdf->pluginType()) {
                _generators[i].Loader(stream, name, bsdf, tree);
                found = true;
                break;
            }
        }

        if (!found) {
            IG_LOG(L_ERROR) << "No bsdf type '" << bsdf->pluginType() << "' available" << std::endl;
            error = true;
        }
    }

    if (error)
        stream << "  let bsdf_" << LoaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_error_bsdf(surf);" << std::endl;

    return stream.str();
}
} // namespace IG
