#include "GeneratorContext.h"

#include "Logger.h"

namespace IG {

using namespace TPM_NAMESPACE;

std::string GeneratorContext::generateTextureLoadInstruction(const std::string& filename, std::string* tex_hndl) const
{
	std::stringstream sstream;

	const auto c_name = handlePath(filename);
	const auto id	  = makeId(filename);

	sstream << "let image_" << id << " = device.load_image(\"" << c_name.string() << "\"); "
			<< "let tex_" << id << " = make_texture(math, make_repeat_border(), make_bilinear_filter(), image_" << id << ");";

	if (tex_hndl)
		*tex_hndl = "let tex_" + id;

	return sstream.str();
}

void GeneratorContext::registerTexturesFromBSDF(const std::shared_ptr<TPMObject>& obj)
{
	for (const auto& child : obj->namedChildren()) {
		if (child.second->type() == OT_TEXTURE)
			Environment.Textures.insert(child.second);
		else if (child.second->type() == OT_BSDF)
			registerTexturesFromBSDF(child.second);
	}

	for (const auto& child : obj->anonymousChildren()) {
		if (child->type() == OT_TEXTURE)
			Environment.Textures.insert(child);
		else if (child->type() == OT_BSDF)
			registerTexturesFromBSDF(child);
	}
}

void GeneratorContext::registerTexturesFromLight(const std::shared_ptr<TPMObject>& obj)
{
	for (const auto& child : obj->namedChildren()) {
		if (child.second->type() == OT_TEXTURE)
			Environment.Textures.insert(child.second);
	}
}

std::string GeneratorContext::extractMaterialPropertyColor(const std::shared_ptr<TPMObject>& obj, const std::string& propname, float def) const
{
	std::stringstream sstream;

	auto prop = obj->property(propname);
	if (prop.isValid()) {
		switch (prop.type()) {
		case PT_INTEGER:
			sstream << "make_gray_color(" << prop.getInteger() << ")";
			break;
		case PT_NUMBER:
			sstream << "make_gray_color(" << prop.getNumber() << ")";
			break;
		case PT_RGB: {
			auto v_rgb = prop.getRGB();
			sstream << "make_color(" << v_rgb.r << ", " << v_rgb.g << ", " << v_rgb.b << ")";
		} break;
		case PT_SPECTRUM: {
			// TODO
			IG_LOG(L_WARNING) << "No support for spectrums" << std::endl;
			sstream << "black";
		} break;
		case PT_BLACKBODY: {
			// TODO
			IG_LOG(L_WARNING) << "No support for blackbodies" << std::endl;
			sstream << "black";
		} break;
		default:
			IG_LOG(L_WARNING) << "Unknown property type for '" << propname << "'" << std::endl;
			sstream << "black";
			break;
		}
	} else {
		auto tex = obj->namedChild(propname);
		if (!tex) {
			sstream << "make_gray_color(" << def << ")";
		} else {
			if (tex->type() == OT_TEXTURE) {
				sstream << extractTexture(tex);
			} else {
				IG_LOG(L_WARNING) << "Invalid child type" << std::endl;
				sstream << "black";
			}
		}
	}

	return sstream.str();
}

std::string GeneratorContext::extractMaterialPropertyNumber(const std::shared_ptr<TPMObject>& obj, const std::string& propname, float def) const
{
	std::stringstream sstream;

	auto prop = obj->property(propname);
	if (prop.isValid()) {
		switch (prop.type()) {
		case PT_INTEGER:
			sstream << prop.getInteger();
			break;
		case PT_NUMBER:
			sstream << prop.getNumber();
			break;
		case PT_RGB: {
			auto v_rgb = prop.getRGB();
			if (v_rgb.r == v_rgb.g && v_rgb.r == v_rgb.b) { // Same value everywhere
				sstream << v_rgb.r;
			} else {
				// TODO
				IG_LOG(L_WARNING) << "No support for rgb number values" << std::endl;
				sstream << def;
			}
		} break;
		case PT_SPECTRUM: {
			// TODO
			IG_LOG(L_WARNING) << "No support for spectrum number values" << std::endl;
			sstream << def;
		} break;
		case PT_BLACKBODY: {
			// TODO
			IG_LOG(L_WARNING) << "No support for blackbody number values" << std::endl;
			sstream << def;
		} break;
		default:
			IG_LOG(L_WARNING) << "Unknown property type for '" << propname << "'" << std::endl;
			sstream << "black";
			break;
		}
	} else {
		auto tex = obj->namedChild(propname);
		if (!tex) {
			sstream << def;
		} else {
			if (tex->type() == OT_TEXTURE) {
				sstream << extractTexture(tex) << ".x";
			} else {
				IG_LOG(L_WARNING) << "Invalid child type" << std::endl;
				sstream << def;
			}
		}
	}

	return sstream.str();
}

std::string GeneratorContext::extractTexture(const std::shared_ptr<TPMObject>& tex) const
{
	std::stringstream sstream;
	if (tex->pluginType() == "bitmap") {
		std::string filename = tex->property("filename").getString();
		if (filename.empty()) {
			IG_LOG(L_WARNING) << "Invalid texture found" << std::endl;
			sstream << "black";
		} else {
			sstream << "tex_" << makeId(filename) << "(vec4_to_2(surf.attr(0)))";
		}
	} else if (tex->pluginType() == "checkerboard") {
		auto uscale = tex->property("uscale").getNumber(1);
		auto vscale = tex->property("vscale").getNumber(1);
		sstream << "eval_checkerboard_texture(math, make_repeat_border(), "
				<< extractMaterialPropertyColor(tex, "color0", 0.4f) << ", "
				<< extractMaterialPropertyColor(tex, "color1", 0.2f) << ", ";
		if (uscale != 1 || vscale != 1)
			sstream << "vec2_mul(vec4_to_2(surf.attr(0)), make_vec2(" << uscale << ", " << vscale << "))";
		else
			sstream << "vec4_to_2(surf.attr(0))";

		sstream << ")";
	} else {
		IG_LOG(L_WARNING) << "Invalid texture type '" << tex->pluginType() << "'" << std::endl;
	}
	return sstream.str();
}

} // namespace IG