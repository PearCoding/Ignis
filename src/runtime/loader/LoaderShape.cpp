#define IG_PARALLEL_LOAD

#include "LoaderShape.h"
#include "Loader.h"
#include "Logger.h"
#include "StringUtils.h"
#include "shape/SphereProvider.h"
#include "shape/TriMeshProvider.h"

#include <algorithm>
#include <chrono>
#include <sstream>

#ifdef IG_PARALLEL_LOAD
#include <tbb/parallel_for_each.h>
#endif

namespace IG {

static const struct ShapeProviderEntry {
    const char* Name;
    const char* Provider;
} _providers[] = {
    { "triangle", "trimesh" },
    { "rectangle", "trimesh" },
    { "cube", "trimesh" },
    { "box", "trimesh" },
    { "sphere", "sphere" },
    { "icosphere", "trimesh" },
    { "uvsphere", "trimesh" },
    { "cylinder", "trimesh" },
    { "cone", "trimesh" },
    { "disk", "trimesh" },
    { "gauss", "trimesh" },
    { "gauss_lobe", "trimesh" },
    { "obj", "trimesh" },
    { "ply", "trimesh" },
    { "mitsuba", "trimesh" },
    { "external", "trimesh" },
    { "inline", "trimesh" },
    { "", nullptr }
};

static const ShapeProviderEntry* getShapeProviderEntry(const std::string& name)
{
    const std::string lower_name = to_lowercase(name);
    for (size_t i = 0; _providers[i].Provider; ++i) {
        if (_providers[i].Name == lower_name)
            return &_providers[i];
    }
    IG_LOG(L_ERROR) << "No shape type '" << name << "' available" << std::endl;
    return nullptr;
}

void LoaderShape::prepare(const LoaderContext& ctx)
{
    // Check which shape provider we need
    for (const auto& ent : ctx.Options.Scene->entities()) {
        const auto shapeName = ent.second->property("shape").getString();
        if (shapeName.empty())
            continue;

        const auto shape = ctx.Options.Scene->shape(shapeName);
        if (!shape)
            continue;

        const auto entry = getShapeProviderEntry(shape->pluginType());
        if (!entry)
            continue;

        const auto it = mShapeProviders.find(entry->Provider);
        if (it == mShapeProviders.end()) {
            if (std::string_view(entry->Provider) == "trimesh") {
                mShapeProviders[entry->Provider] = std::make_unique<TriMeshProvider>();
            } else if (std::string_view(entry->Provider) == "sphere") {
                mShapeProviders[entry->Provider] = std::make_unique<SphereProvider>();
            } else {
                IG_ASSERT(false, "Shape provider entries and implementation is incomplete!");
            }
        }
    }
}

bool LoaderShape::load(LoaderContext& ctx)
{
    ShapeMTAccessor acc;

    // Make sure this table is preloaded
    ctx.Database.DynTables.emplace("shapes", DynTable{});

    const auto load_shape = [&](const std::string& name, SceneObject* child) {
        auto entry = getShapeProviderEntry(child->pluginType());
        if (!entry)
            return;

        if (const auto it = mShapeProviders.find(entry->Provider); it != mShapeProviders.end())
            it->second->handle(ctx, acc, name, *child);
    };

    // Start loading!
    IG_LOG(L_DEBUG) << "Loading shapes..." << std::endl;
    const auto start1 = std::chrono::high_resolution_clock::now();
#ifdef IG_PARALLEL_LOAD
    tbb::parallel_for_each(ctx.Options.Scene->shapes().begin(), ctx.Options.Scene->shapes().end(),
                           [&](const Scene::ObjectMap::value_type& value) {
                               load_shape(value.first, value.second.get());
                           });
#else
    for (auto it = ctx.Options.Scene->shapes().begin(); it != ctx.Options.Scene->shapes().end(); ++it)
        load_shape(it->first, it->second.get());
#endif
    IG_LOG(L_DEBUG) << "Loading of shapes took " << (std::chrono::high_resolution_clock::now() - start1) << std::endl;

    return !ctx.HasError;
}

uint32 LoaderShape::addShape(const std::string& name, const Shape& shape)
{
    const std::lock_guard<std::mutex> lock(mAccessMutex);
    auto it = mIDs.find(name);
    if (it != mIDs.end())
        return it->second;

    const uint32 id = (uint32)mIDs.size();
    mIDs[name]      = id;

    mShapes.emplace_back(shape);

    return id;
}

void LoaderShape::addTriShape(uint32 id, const TriShape& shape)
{
    const std::lock_guard<std::mutex> lock(mAccessMutex);
    mTriShapes[id] = shape;
}

void LoaderShape::addPlaneShape(uint32 id, const PlaneShape& shape)
{
    const std::lock_guard<std::mutex> lock(mAccessMutex);
    mPlaneShapes[id] = shape;
}

void LoaderShape::addSphereShape(uint32 id, const SphereShape& shape)
{
    const std::lock_guard<std::mutex> lock(mAccessMutex);
    mSphereShapes[id] = shape;
}
} // namespace IG