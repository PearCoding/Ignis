#pragma once

#include "IG_Config.h"
#include <mutex>

namespace IG {
class IG_LIB CacheManager {
    using HashMap = std::unordered_map<std::string, std::string>;

public:
    CacheManager(const Path& cache_dir);
    ~CacheManager();

    inline void enable(bool b) { mEnabled = b; }
    inline bool isEnabled() const { return mEnabled; }
    inline const Path& directory() const { return mCacheDir; }

    void sync();

    bool check(const std::string& name, const std::string& hash) const;
    void update(const std::string& name, const std::string& hash);
    bool checkAndUpdate(const std::string& name, const std::string& hash);

private:
    HashMap load();
    void merge(const HashMap& map);
    void save(const HashMap& map);

    mutable std::mutex mMutex;
    bool mEnabled;
    HashMap mHashMap;
    Path mCacheDir;
};
} // namespace IG