#include "LoaderBSDF.h"
#include "Loader.h"
#include "LoaderUtils.h"
#include "Logger.h"
#include "ShadingTree.h"
#include "StringUtils.h"

#include "bsdf/BlendBSDF.h"
#include "bsdf/ConductorBSDF.h"
#include "bsdf/NeuralBSDF.h"
#include "bsdf/DJMeasuredBSDF.h"
#include "bsdf/DielectricBSDF.h"
#include "bsdf/DiffuseBSDF.h"
#include "bsdf/ErrorBSDF.h"

namespace IG {
struct LoaderBSDFSingleton {
    std::unordered_map<std::string, LoaderBSDF::RegisterBSDFCallback> Loaders;

    static inline LoaderBSDFSingleton& instance()
    {
        static LoaderBSDFSingleton sSingleton;
        return sSingleton;
    }
};

void LoaderBSDF::registerBSDFHandler(const std::string& name, RegisterBSDFCallback callback)
{
    IG_ASSERT(LoaderBSDFSingleton::instance().Loaders.count(name) == 0, "Trying to register a BSDF twice!");
    LoaderBSDFSingleton::instance().Loaders[name] = callback;
}

void LoaderBSDF::prepare(const LoaderContext& ctx)
{
    for (const auto& pair : ctx.Options.Scene->bsdfs()) {
        const std::string name = pair.first;
        const auto bsdf        = pair.second;
        if (auto it = LoaderBSDFSingleton::instance().Loaders.find(bsdf->pluginType()); it != LoaderBSDFSingleton::instance().Loaders.end()) {
            mAvailableBSDFs.emplace(name, it->second(name, bsdf));
        } else {
            IG_LOG(L_ERROR) << "No bsdf type '" << bsdf->pluginType() << "' for '" << name << "' available" << std::endl;
            mAvailableBSDFs.emplace(name, std::make_shared<ErrorBSDF>(name));
        }
    }
}

std::string LoaderBSDF::generate(const std::string& name, ShadingTree& tree)
{
    std::stringstream stream;
    auto it = mAvailableBSDFs.find(name);
    if (it == mAvailableBSDFs.end()) {
        IG_LOG(L_ERROR) << "Unknown bsdf '" << name << "'" << std::endl;
        stream << ErrorBSDF::inlineError(tree.getClosureID(name)) << std::endl;
        return stream.str();
    }

    mGeneratedBSDFs.insert(name);

    it->second->serialize(BSDF::SerializationInput{ stream, tree });

    return stream.str();
}
} // namespace IG
