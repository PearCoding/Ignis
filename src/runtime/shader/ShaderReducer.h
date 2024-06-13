#pragma once

#include "IG_Config.h"

namespace IG {
class ShaderReducer {
public:
    using ShaderKey = std::tuple<std::string, std::string>;

    inline std::string registerGroupID(const std::string& id, const std::string& script, const std::string& function)
    {
        const auto key = ShaderKey{ script, function };
        mIds.insert(std::make_pair(key, id));

        const auto hash = hash_tuple<ShaderKey>()(key);
        return std::to_string(hash);
    }

    [[nodiscard]] inline std::string getGroupID(const std::string& script, const std::string& function) const
    {
        const auto key  = ShaderKey{ script, function };
        const auto hash = hash_tuple<ShaderKey>()(key);
        return std::to_string(hash);
    }

    [[nodiscard]] inline const std::unordered_multimap<ShaderKey, std::string, hash_tuple<ShaderKey>>& groups() const { return mIds; }

    [[nodiscard]] inline size_t numberOfEntries() const { return mIds.size(); }
    [[nodiscard]] inline size_t computeNumberOfUniqueEntries() const
    {
        size_t c = 0;
        for (auto it = mIds.begin(); it != mIds.end();) {
            const auto key = it->first;
            ++c;

            // Skip other elements with the same key
            while (++it != mIds.end() && it->first == key)
                ;
        }
        return c;
    }

private:
    std::unordered_multimap<ShaderKey, std::string, hash_tuple<ShaderKey>> mIds;
};
} // namespace IG