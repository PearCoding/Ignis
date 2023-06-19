#pragma once

#include "IG_Config.h"

#include <unordered_set>

namespace IG {
class BSDF;
class LoaderContext;
class ShadingTree;
class SceneObject;

class LoaderBSDF {
public:
    void prepare(const LoaderContext& ctx);

    [[nodiscard]] std::string generate(const std::string& name, ShadingTree& tree);

    [[nodiscard]] inline bool checkIfGenerated(const std::string& name) const { return mGeneratedBSDFs.count(name) > 0; }
    [[nodiscard]] inline size_t bsdfCount() const { return mAvailableBSDFs.size(); }

    using RegisterBSDFCallback = std::shared_ptr<BSDF> (*)(const std::string&, const std::shared_ptr<SceneObject>&);
    static void registerBSDFHandler(const std::string& name, RegisterBSDFCallback callback);

private:
    std::unordered_map<std::string, std::shared_ptr<BSDF>> mAvailableBSDFs; // All available bsdfs, not necessarily the ones loaded at the end!
    std::unordered_set<std::string> mGeneratedBSDFs;                        // Lookup table to check if material was loaded or not
};

template <typename T>
struct BSDFRegister {
    static inline std::shared_ptr<BSDF> callback(const std::string& n, const std::shared_ptr<SceneObject>& o) { return std::make_shared<T>(n, o); }
    template <typename K, typename... Kother>
    inline BSDFRegister(K a, Kother... args)
    {
        LoaderBSDF::registerBSDFHandler(a, callback);
        BSDFRegister<T>(std::forward<Kother>(args)...);
    }
    template <typename K>
    inline BSDFRegister(K a)
    {
        LoaderBSDF::registerBSDFHandler(a, callback);
    }
};

struct BSDFRawRegister {
    template <typename K, typename... Kother>
    inline BSDFRawRegister(LoaderBSDF::RegisterBSDFCallback clb, K a, Kother... args)
    {
        LoaderBSDF::registerBSDFHandler(a, clb);
        BSDFRawRegister(clb, std::forward<Kother>(args)...);
    }
    template <typename K>
    inline BSDFRawRegister(LoaderBSDF::RegisterBSDFCallback clb, K a)
    {
        LoaderBSDF::registerBSDFHandler(a, clb);
    }
};

} // namespace IG