#include "GlareEvaluator.h"
#include "Runtime.h"
#include "shader/ShaderUtils.h"

namespace IG {
GlareEvaluator::GlareEvaluator(Runtime* runtime)
    : mRuntime(runtime)
    , mMultiplier(5)
    , mVerticalIlluminance(-1)
{
    IG_ASSERT(runtime, "Expected a valid runtime");
    setup();
}

GlareEvaluator::~GlareEvaluator()
{
}

static const char* HandlerSrc = R"(
    let u_width = registry::get_global_parameter_i32("__glare_framebuffer_width", 0);
    let u_height = registry::get_global_parameter_i32("__glare_framebuffer_height", 0);

    let use_custom = u_width > 0 && u_height > 0;

    let width  = if !use_custom { settings.width } else { u_width };
    let height = if !use_custom { settings.height } else { u_height };

    let input = if use_custom {
        let buffer = device.make_buffer_view(userData as &[f32], width * height * 3);
        make_aov_image_from_buffer_readonly(buffer, width, height)
    } else {
        device.load_aov_image("", spi)
    };

    let source_luminance = device.request_buffer("_glare_source_luminance", width * height, 0);

    let camera_eye  = registry::get_global_parameter_vec3("__camera_eye", vec3_expand(0));
    let camera_dir  = registry::get_global_parameter_vec3("__camera_dir", vec3_expand(0));
    let camera_up   = registry::get_global_parameter_vec3("__camera_up" , vec3_expand(0));
    let cam_fisheye = make_fishlens_camera(camera_eye, camera_dir, camera_up, width, height, FisheyeAspectMode::Circular /* Fixed in code */, 0, 100, true);

    let glareSettings = GlareSettings{
        scale = if use_custom { 1:f32 } else { 1 / (settings.iter + 1) as f32 },
        mul   = registry::get_global_parameter_f32("_glare_multiplier", 5),
        vertical_illuminance = registry::get_global_parameter_f32("_glare_vertical_illuminance", -1)
    };

    compute_glare(device, cam_fisheye, input, source_luminance, &glareSettings);
)";

void GlareEvaluator::setup()
{
    std::stringstream shader;
    shader << "#[export] fn ig_pass_main(settings: &Settings) -> () {" << std::endl
           << ShaderUtils::constructDevice(mRuntime->loaderOptions()) << std::endl
           << HandlerSrc
           << "}";

    mPass = mRuntime->createPass(shader.str());
}

std::optional<GlareEvaluator::Result> GlareEvaluator::run()
{
    if (!mPass)
        return {};

    mPass->setParameter("_glare_multiplier", mMultiplier);
    mPass->setParameter("_glare_vertical_illuminance", mVerticalIlluminance);

    mPass->setUserData((void*)mData);

    if (mData == nullptr) {
        mPass->setParameter("__glare_framebuffer_width", (int)0);
        mPass->setParameter("__glare_framebuffer_height", (int)0);
    } else {
        mPass->setParameter("__glare_framebuffer_width", (int)mDataWidth);
        mPass->setParameter("__glare_framebuffer_height", (int)mDataHeight);
    }

    if (!mPass->run())
        return {};

    return Result{
        .DGP                 = mRuntime->parameters().getFloat("glare_dgp"),
        .DGI                 = mRuntime->parameters().getFloat("glare_dgi"),
        .DGImod              = mRuntime->parameters().getFloat("glare_dgi_mod"),
        .DGR                 = mRuntime->parameters().getFloat("glare_dgr"),
        .VCP                 = mRuntime->parameters().getFloat("glare_vcp"),
        .UGR                 = mRuntime->parameters().getFloat("glare_ugr"),
        .UGRexp              = mRuntime->parameters().getFloat("glare_ugr_exp"),
        .UGP                 = mRuntime->parameters().getFloat("glare_ugp"),
        .VerticalIlluminance = mRuntime->parameters().getFloat("glare_vertical_illuminance"),
        .SourceLuminance     = mRuntime->parameters().getFloat("glare_source_luminance"),
        .SourceOmega         = mRuntime->parameters().getFloat("glare_omega"),
        .BackgroundLuminance = mRuntime->parameters().getFloat("glare_background_luminance"),
        .TotalOmega          = mRuntime->parameters().getFloat("total_omega"),
        .TotalLuminance      = mRuntime->parameters().getFloat("total_lum")
    };
}
} // namespace IG