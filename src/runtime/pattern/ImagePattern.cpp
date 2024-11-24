#include "ImagePattern.h"
#include "Image.h"
#include "Logger.h"
#include "SceneObject.h"
#include "loader/LoaderUtils.h"
#include "loader/ShadingTree.h"

namespace IG {
ImagePattern::ImagePattern(const std::string& name, const std::shared_ptr<SceneObject>& object)
    : Pattern(name, "image")
    , mObject(object)
{
}

void ImagePattern::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    const Path filename           = input.Tree.context().getPath(*mObject, "filename");
    const std::string filter_type = mObject->property("filter_type").getString("bicubic");
    const Transformf transform    = mObject->property("transform").getTransform();
    const bool force_unpacked     = mObject->property("force_unpacked").getBool(false); // Force the use of unpacked (float) images
    const bool linear             = mObject->property("linear").getBool(false);         // Hint that the image is already in linear. Only important if image type is not EXR or HDR, as they are always given in linear

    size_t res_id = input.Tree.context().registerExternalResource(filename);

    std::string filter = "make_bicubic_filter()";
    if (filter_type == "bilinear")
        filter = "make_bilinear_filter()";
    else if (filter_type == "nearest")
        filter = "make_nearest_filter()";

    const auto getWrapMode = [](const std::string& str) {
        if (str == "mirror")
            return "make_mirror_border()";
        else if (str == "clamp")
            return "make_clamp_border()";
        else
            return "make_repeat_border()";
    };

    std::string wrap;
    if (mObject->property("wrap_mode_u").isValid()) {
        std::string wrap_u = getWrapMode(mObject->property("wrap_mode_u").getString("repeat"));
        std::string wrap_v = getWrapMode(mObject->property("wrap_mode_v").getString("repeat"));
        if (wrap_u == wrap_v)
            wrap = wrap_u;
        else
            wrap = "make_split_border(" + wrap_u + ", " + wrap_v + ")";
    } else {
        wrap = getWrapMode(mObject->property("wrap_mode").getString("repeat"));
    }

    const std::string tex_id = input.Tree.getClosureID(name());

    // Anonymize lookup by using the local registry
    input.Tree.addComputedInteger(tex_id + "_image_index", (int32)res_id, ShadingTree::IntegerOptions::Dynamic());

    const size_t channel_count = Image::loadResolution(filename).Channels == 1 ? 1 : 4;

    input.Stream << input.Tree.pullHeader();

    if (!force_unpacked && Image::isPacked(filename))
        input.Stream << "  let img_" << tex_id << " = device.load_packed_image_by_id(" << input.Tree.getInline(tex_id + "_image_index") << ", " << channel_count << ", " << (linear ? "true" : "false") << ");" << std::endl;
    else
        input.Stream << "  let img_" << tex_id << " = device.load_image_by_id(" << input.Tree.getInline(tex_id + "_image_index") << ", " << channel_count << ");" << std::endl;

    input.Stream << "  let tex_" << tex_id << " : Texture = make_image_texture("
                 << wrap << ", "
                 << filter << ", "
                 << "img_" << tex_id << ", "
                 << LoaderUtils::inlineTransformAs2d(transform) << ");" << std::endl;

    input.Tree.endClosure();
}

std::pair<size_t, size_t> ImagePattern::computeResolution(ShadingTree& tree) const
{
    const Path filename = tree.context().getPath(*mObject, "filename");

    try {
        const auto res = Image::loadResolution(filename);
        return { res.Width, res.Height };
    } catch (const ImageLoadException& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        return { 1, 1 };
    }
}
} // namespace IG