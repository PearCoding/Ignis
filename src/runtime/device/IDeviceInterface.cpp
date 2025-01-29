
#include "IDeviceInterface.h"
#include "DeviceUtils.h"

#include "generated_interface.h"

namespace IG {
static IDeviceInterface* sCurrentDevice = nullptr;
IDeviceInterface* IDeviceInterface::getCurrentDevice() { return sCurrentDevice; }
void IDeviceInterface::setCurrentDevice(IDeviceInterface* interface) { sCurrentDevice = interface; }

constexpr size_t MinPrimaryStreamSize   = (sizeof(PrimaryStream) - sizeof(PrimaryStream::payload)) / sizeof(PrimaryStream::ent_id);
constexpr size_t MinSecondaryStreamSize = (sizeof(SecondaryStream) - sizeof(SecondaryStream::payload)) / sizeof(SecondaryStream::mat_id);

template <typename T>
inline void getStream(T* dev_stream, IDeviceInterface::DeviceStreamProxy<float>& stream, size_t min_components)
{
    static_assert(std::is_standard_layout<T>::value, "Expected stream to be plain old data");
    static_assert((sizeof(T) % sizeof(float*)) == 0, "Expected stream size to be multiple of pointer size");

    float* ptr      = stream.DataPtr;
    size_t capacity = stream.BlockSize;

    auto r_ptr = reinterpret_cast<float**>(dev_stream);
    for (size_t i = 0; i <= min_components; ++i) // The last part of the stream is used by the payload
        r_ptr[i] = ptr + i * capacity;
}

static inline ParameterSet* getParameterSet(IDeviceInterface* device, bool global)
{
    return global ? device->getCurrentGlobalRegistry() : device->getCurrentLocalRegistry();
}

static inline DynTableData assignDynTable(const IDeviceInterface::DynTableProxy& tbl)
{
    DynTableData devtbl;
    devtbl.count  = tbl.EntryCount;
    devtbl.header = const_cast<::LookupEntry*>((const ::LookupEntry*)tbl.LookupEntries);
    devtbl.size   = tbl.DataSize;
    devtbl.start  = const_cast<uint8_t*>(tbl.DataPtr);
    return devtbl;
}
} // namespace IG

using namespace IG;
extern "C" {
IG_EXPORT void ignis_get_film_data(float** pixels, int* width, int* height)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");

    auto framebuffer = device->getFramebuffer();
    *pixels          = framebuffer.DataPtr;
    *width           = (int)framebuffer.Width;
    *height          = (int)framebuffer.Height;
}

IG_EXPORT void ignis_get_aov_image(const char* name, float** aov_pixels)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    *aov_pixels = device->loadAOVImageForDevice(name).DataPtr;
}

IG_EXPORT void ignis_get_work_info(WorkInfo* info)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    const auto workSize = device->workSize();
    info->width         = (int)std::get<0>(workSize);
    info->height        = (int)std::get<1>(workSize);

    info->advanced_shadows                = device->currentRenderSettings().info.ShadowHandlingMode == IG::ShadowHandlingMode::Advanced;
    info->advanced_shadows_with_materials = device->currentRenderSettings().info.ShadowHandlingMode == IG::ShadowHandlingMode::AdvancedWithMaterials;
    info->framebuffer_locked              = device->currentRenderSettings().info.LockFramebuffer;
}

IG_EXPORT void ignis_load_bvh2_ent(const char* prim_type, Node2** nodes, EntityLeaf1** objs)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->loadEntityBVH(IDeviceInterface::BVHType::BVH2, prim_type, (void**)nodes, (void**)objs);
}

IG_EXPORT void ignis_load_bvh4_ent(const char* prim_type, Node4** nodes, EntityLeaf1** objs)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->loadEntityBVH(IDeviceInterface::BVHType::BVH4, prim_type, (void**)nodes, (void**)objs);
}

IG_EXPORT void ignis_load_bvh8_ent(const char* prim_type, Node8** nodes, EntityLeaf1** objs)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->loadEntityBVH(IDeviceInterface::BVHType::BVH8, prim_type, (void**)nodes, (void**)objs);
}

IG_EXPORT void ignis_load_dyntable(const char* name, DynTableData* dtb)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    auto proxy = device->loadDynTable(name);
    *dtb       = assignDynTable(proxy);
}

IG_EXPORT void ignis_load_fixtable(const char* name, uint8_t** data, int32_t* size)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    auto proxy = device->loadFixTable(name);
    *data      = const_cast<uint8_t*>(proxy.DataPtr);
    *size      = (int32_t)proxy.SizeInBytes;
}

IG_EXPORT void ignis_load_rays(StreamRay** list)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    *list = (StreamRay*)(device->loadRayList());
}

IG_EXPORT void ignis_load_image(const char* file, float** pixels, int32_t* width, int32_t* height, int32_t expected_channels)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    auto proxy = device->loadImageFromFile(file, expected_channels);
    *pixels    = const_cast<float*>(proxy.DataPtr);
    *width     = (int32_t)proxy.Width;
    *height    = (int32_t)proxy.Height;
}

IG_EXPORT void ignis_load_image_by_id(int32_t id, float** pixels, int32_t* width, int32_t* height, int32_t expected_channels)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    return ignis_load_image(device->lookupResource(id).data(), pixels, width, height, expected_channels);
}

IG_EXPORT void ignis_load_packed_image(const char* file, uint8_t** pixels, int32_t* width, int32_t* height, int32_t expected_channels, bool linear)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    auto proxy = device->loadPackedImageFromFile(file, expected_channels, linear);
    *pixels    = const_cast<uint8_t*>(proxy.DataPtr);
    *width     = (int32_t)proxy.Width;
    *height    = (int32_t)proxy.Height;
}

IG_EXPORT void ignis_load_packed_image_by_id(int32_t id, uint8_t** pixels, int32_t* width, int32_t* height, int32_t expected_channels, bool linear)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    return ignis_load_packed_image(device->lookupResource(id).data(), pixels, width, height, expected_channels, linear);
}

IG_EXPORT void ignis_load_buffer(const char* file, uint8_t** data, int32_t* size)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    auto proxy = device->loadBufferFromFile(file);
    *data      = const_cast<uint8_t*>(proxy.DataPtr);
    *size      = (int32_t)proxy.SizeInBytes;
}

IG_EXPORT void ignis_load_buffer_by_id(int32_t id, uint8_t** data, int32_t* size)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    return ignis_load_buffer(device->lookupResource(id).data(), data, size);
}

IG_EXPORT void ignis_request_buffer(const char* name, uint8_t** data, int size, int flags)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    auto proxy = device->requestBuffer(name, size, flags);
    *data      = const_cast<uint8_t*>(proxy.DataPtr);

    IG_ASSERT(proxy.SizeInBytes >= (size_t)size, "Expected data allocation to allocate enough memory");
}

IG_EXPORT void ignis_dbg_dump_buffer(const char* name, const char* filename)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->saveBuffer(name, filename);
}

IG_EXPORT void ignis_get_temporary_storage_host(TemporaryStorageHost* temp)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    const auto proxy          = device->getTemporaryStorageHost();
    temp->ray_begins          = const_cast<int32_t*>(proxy.RayBeginsPtr);
    temp->ray_ends            = const_cast<int32_t*>(proxy.RayEndsPtr);
    temp->entity_per_material = const_cast<int32_t*>(device->currentSceneSettings().entity_per_material->data());
}

IG_EXPORT void ignis_get_primary_stream(int id, PrimaryStream* primary, int size)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    auto stream = device->getStream(IDeviceInterface::StreamType::Primary, id, size, IG::MinPrimaryStreamSize);
    IG::getStream(primary, stream, IG::MinPrimaryStreamSize);
}

IG_EXPORT void ignis_get_primary_stream_const(int id, PrimaryStream* primary)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    auto stream = device->getStream(IDeviceInterface::StreamType::Primary, id);
    IG::getStream(primary, stream, IG::MinPrimaryStreamSize);
}

IG_EXPORT void ignis_get_secondary_stream(int id, SecondaryStream* secondary, int size)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    auto stream = device->getStream(IDeviceInterface::StreamType::Secondary, id, size, IG::MinSecondaryStreamSize);
    IG::getStream(secondary, stream, IG::MinSecondaryStreamSize);
}

IG_EXPORT void ignis_get_secondary_stream_const(int id, SecondaryStream* secondary)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    auto stream = device->getStream(IDeviceInterface::StreamType::Secondary, id);
    IG::getStream(secondary, stream, IG::MinSecondaryStreamSize);
}

IG_EXPORT void ignis_gpu_swap_primary_streams()
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->swapGPUStreams(IDeviceInterface::StreamType::Primary);
}

IG_EXPORT void ignis_gpu_swap_secondary_streams()
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->swapGPUStreams(IDeviceInterface::StreamType::Secondary);
}

IG_EXPORT void ignis_register_thread()
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    enableFastMathModeForThread();
    device->registerThread();
}

IG_EXPORT void ignis_unregister_thread()
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->unregisterThread();
    disableFastMathModeForThread();
}

IG_EXPORT void ignis_handle_traverse_primary(int size)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->runTraversalShader(IDeviceInterface::TraversalStage::Primary, size);
}

IG_EXPORT void ignis_handle_traverse_secondary(int size)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->runTraversalShader(IDeviceInterface::TraversalStage::Secondary, size);
}

IG_EXPORT int ignis_handle_ray_generation(int next_id, int size, int xmin, int ymin, int xmax, int ymax)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    return device->runRayGenerationShader(next_id, size, xmin, ymin, xmax, ymax);
}

IG_EXPORT void ignis_handle_miss_shader(int first, int last)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->runMaterialShader(-1, first, last);
}

IG_EXPORT void ignis_handle_hit_shader(int entity_id, int first, int last)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->runMaterialShader(entity_id, first, last);
}

IG_EXPORT void ignis_handle_advanced_shadow_shader(int material_id, int first, int last, bool is_hit)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    if (device->currentRenderSettings().info.ShadowHandlingMode == IG::ShadowHandlingMode::Advanced)
        device->runAdvancedShadowShader(0 /* Fix to 0 */, first, last, is_hit);
    else
        device->runAdvancedShadowShader(material_id, first, last, is_hit);
}

IG_EXPORT void ignis_handle_callback_shader(int type)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->runCallbackShader(type);
}

// Registry stuff
IG_EXPORT int ignis_get_parameter_i32(const char* name, int32_t def, bool global)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    return getParameterSet(device, global)->getInt(name, def);
}

IG_EXPORT float ignis_get_parameter_f32(const char* name, float def, bool global)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    return getParameterSet(device, global)->getFloat(name, def);
}

IG_EXPORT void ignis_get_parameter_vector(const char* name, float defX, float defY, float defZ, float* x, float* y, float* z, bool global)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    const Vector3f data = getParameterSet(device, global)->getVector(name, Vector3f(defX, defY, defZ));

    *x = data.x();
    *y = data.y();
    *z = data.z();
}

IG_EXPORT void ignis_get_parameter_color(const char* name, float defR, float defG, float defB, float defA, float* r, float* g, float* b, float* a, bool global)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    const Vector4f data = getParameterSet(device, global)->getColor(name, Vector4f(defR, defG, defB, defA));

    *r = data.x();
    *g = data.y();
    *b = data.z();
    *a = data.w();
}

IG_EXPORT const char* ignis_get_parameter_string(const char* name, const char* def, bool global)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    return getParameterSet(device, global)->getString(name, def).c_str();
}

IG_EXPORT void ignis_set_parameter_i32(const char* name, int32_t value, bool global)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    getParameterSet(device, global)->set(name, value);
}

IG_EXPORT void ignis_set_parameter_f32(const char* name, float value, bool global)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    getParameterSet(device, global)->set(name, value);
}

IG_EXPORT void ignis_set_parameter_vector(const char* name, float valueX, float valueY, float valueZ, bool global)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    getParameterSet(device, global)->set(name, Vector3f(valueX, valueY, valueZ));
}

IG_EXPORT void ignis_set_parameter_color(const char* name, float valueR, float valueG, float valueB, float valueA, bool global)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    getParameterSet(device, global)->set(name, Vector4f(valueR, valueG, valueB, valueA));
}

// Stats
IG_EXPORT void ignis_stats_begin_section(int32_t id)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->beginStatsSection(id);
}

IG_EXPORT void ignis_stats_end_section(int32_t id)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->endStatsSection(id);
}

IG_EXPORT void ignis_stats_add(int32_t id, int32_t value)
{
    IDeviceInterface* device = IDeviceInterface::getCurrentDevice();
    IG_ASSERT(device, "Expected valid interface");
    device->addStatsValue(id, value);
}
}