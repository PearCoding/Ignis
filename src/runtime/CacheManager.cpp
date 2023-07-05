#include "CacheManager.h"
#include "Logger.h"

#include <fstream>

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>

namespace IG {
CacheManager::CacheManager(const Path& cache_dir)
    : mEnabled(true)
    , mHashMap()
    , mCacheDir(cache_dir)
{
    try {
        std::filesystem::create_directories(mCacheDir); // Make sure this directory exists
    } catch (const std::filesystem::filesystem_error&) {
        mCacheDir = std::filesystem::temp_directory_path() / mCacheDir.filename();
        IG_LOG(L_WARNING) << "Could not use " << cache_dir << " as scene cache. Trying to use " << mCacheDir << " instead." << std::endl;
        std::filesystem::create_directories(mCacheDir);
    }
}

CacheManager::~CacheManager()
{
    // Get rid of empty caches
    try {
        if (std::filesystem::exists(mCacheDir) && std::filesystem::is_empty(mCacheDir))
            std::filesystem::remove(mCacheDir);
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore
    }
}

void CacheManager::sync()
{
    if (!mEnabled)
        return;

    std::lock_guard<std::mutex> _guard(mMutex);
    const HashMap map = load();
    merge(map);
    save(mHashMap);
}

bool CacheManager::check(const std::string& name, const std::string& hash) const
{
    if (!mEnabled)
        return false;

    std::lock_guard<std::mutex> _guard(mMutex);

    if (const auto it = mHashMap.find(name); it != mHashMap.end())
        return it->second == hash;
    else
        return false;
}

void CacheManager::update(const std::string& name, const std::string& hash)
{
    if (mEnabled) {
        std::lock_guard<std::mutex> _guard(mMutex);
        mHashMap[name] = hash;
    }
}

bool CacheManager::checkAndUpdate(const std::string& name, const std::string& hash)
{
    if (!mEnabled)
        return false;

    if (!check(name, hash)) {
        update(name, hash);
        return false;
    } else {
        return true;
    }
}

CacheManager::HashMap CacheManager::load()
{
    const Path file = mCacheDir / "cache.json";
    if (!std::filesystem::exists(file))
        return {}; // No cache file means nothing was cached yet

    std::ifstream ifs(file.generic_u8string());
    if (!ifs.good()) {
        IG_LOG(L_ERROR) << "Could not open file '" << file << "'" << std::endl;
        return {};
    }

    rapidjson::IStreamWrapper isw(ifs);

    rapidjson::Document doc;
    if (doc.ParseStream(isw).HasParseError()) {
        IG_LOG(L_ERROR) << "JSON[" << doc.GetErrorOffset() << "]: " << rapidjson::GetParseError_En(doc.GetParseError()) << std::endl;
        return {};
    }

    if (!doc.IsObject()) {
        IG_LOG(L_ERROR) << "JSON: Expected root element to be an object" << std::endl;
        return {};
    }

    HashMap map;
    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
        if (it->value.IsString())
            map[it->name.GetString()] = it->value.GetString();
    }

    return map;
}

void CacheManager::merge(const HashMap& map)
{
    for (const auto& pair : map)
        mHashMap.try_emplace(pair.first, pair.second);
}

void CacheManager::save(const HashMap& map)
{
    if (map.empty())
        return; // Nothing to cache

    const Path file = mCacheDir / "cache.json";
    std::ofstream ofs(file.generic_u8string());
    if (!ofs.good()) {
        IG_LOG(L_ERROR) << "Could not open file '" << file << "'" << std::endl;
        return;
    }

    rapidjson::Document doc;
    doc.SetObject();

    for (const auto& pair : map)
        doc.AddMember(rapidjson::StringRef(pair.first), rapidjson::StringRef(pair.second), doc.GetAllocator());

    rapidjson::OStreamWrapper osw(ofs);
    rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
    doc.Accept(writer);
}

} // namespace IG