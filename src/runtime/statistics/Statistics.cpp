#include "Statistics.h"
#include "Color.h"
#include "Logger.h"
#include "config/Version.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/istreamwrapper.h>

namespace IG {
constexpr size_t InitialStreamSize = 4096;
constexpr size_t MaxStreamEntries  = InitialStreamSize * 1024;

Statistics::Statistics()
    : mCurrentStream(0)
    , mQuantities()
{
    std::fill(mQuantities.begin(), mQuantities.end(), 0);
    mStream.reserve(InitialStreamSize);
}

void Statistics::record(const Timestamp& timestamp, bool addToStream)
{
    if (addToStream && mStream.size() < MaxStreamEntries)
        mStream.push_back(timestamp);

    std::visit([&](auto&& type) {
        using T = std::decay_t<decltype(type)>;
        if constexpr (std::is_same_v<T, SmallShaderKey>) {
            auto& stats = mShaders.try_emplace(type).first->second;
            stats.count += 1;
            stats.elapsedMS += (timestamp.offsetEndMS - timestamp.offsetStartMS);
            stats.workload += timestamp.workload;
            stats.minWorkload = std::min(stats.minWorkload, timestamp.workload);
            stats.maxWorkload = std::max(stats.maxWorkload, timestamp.workload);
        } else if constexpr (std::is_same_v<T, SectionType>) {
            auto& stats = mSections[(size_t)type];
            stats.count += 1;
            stats.elapsedMS += (timestamp.offsetEndMS - timestamp.offsetStartMS);
        }
    },
               timestamp.type);
}

void Statistics::finalizeStream()
{
    // Sort based on the start
    std::sort(mStream.begin(), mStream.end(),
              [](const Timestamp& a, const Timestamp& b) { return a.offsetStartMS < b.offsetStartMS; });
}

void Statistics::add(Quantity quantity, uint64 value)
{
    mQuantities[(size_t)quantity] += value;
}

Statistics::ShaderStats& Statistics::ShaderStats::operator+=(const Statistics::ShaderStats& other)
{
    elapsedMS += other.elapsedMS;
    count += other.count;
    workload += other.workload;
    maxWorkload = std::max(maxWorkload, other.maxWorkload);
    minWorkload = std::min(minWorkload, other.minWorkload);

    return *this;
}

Statistics::SectionStats& Statistics::SectionStats::operator+=(const Statistics::SectionStats& other)
{
    elapsedMS += other.elapsedMS;
    count += other.count;

    return *this;
}

void Statistics::add(const Statistics& other)
{
    for (const auto& p : other.mShaders)
        mShaders[p.first] += p.second;

    for (size_t i = 0; i < other.mQuantities.size(); ++i)
        mQuantities[i] += other.mQuantities[i];

    for (size_t i = 0; i < other.mSections.size(); ++i)
        mSections[i] += other.mSections[i];

    mStream.insert(mStream.end(), other.mStream.begin(), other.mStream.end());
}

Statistics::MeanEntry Statistics::entry(ShaderType type, uint32 sub_id) const
{
    if (const auto it = mShaders.find(SmallShaderKey(type, sub_id)); it != mShaders.end()) {
        return MeanEntry{
            it->second.elapsedMS,
            it->second.count,
            it->second.workload,
            it->second.minWorkload,
            it->second.maxWorkload
        };
    } else {
        return MeanEntry();
    }
}

Statistics::MeanEntry Statistics::entry(SectionType type) const
{
    const auto& s = mSections.at((size_t)type);
    return MeanEntry{
        s.elapsedMS,
        s.count,
        0, 0, 0
    };
}

const static std::unordered_map<ShaderType, const char*> ShaderTypeName = {
    { ShaderType::Device, "Device" },
    { ShaderType::PrimaryTraversal, "PrimaryTraversal" },
    { ShaderType::SecondaryTraversal, "SecondaryTraversal" },
    { ShaderType::RayGeneration, "RayGeneration" },
    { ShaderType::Hit, "Hit" },
    { ShaderType::Miss, "Miss" },
    { ShaderType::AdvancedShadowHit, "AdvancedShadowHit" },
    { ShaderType::AdvancedShadowMiss, "AdvancedShadowMiss" },
    { ShaderType::Callback, "Callback" },
    { ShaderType::Tonemap, "Tonemap" },
    { ShaderType::Glare, "Glare" },
    { ShaderType::ImageInfo, "ImageInfo" },
    { ShaderType::Bake, "Bake" },
};

const static std::unordered_map<SectionType, const char*> SectionTypeName = {
    { SectionType::GPUSortPrimary, "GPUSortPrimary" },
    { SectionType::GPUSortSecondary, "GPUSortSecondary" },
    { SectionType::GPUCompactPrimary, "GPUCompactPrimary" },
    { SectionType::GPUSortPrimaryReset, "GPUSortPrimaryReset" },
    { SectionType::GPUSortPrimaryCount, "GPUSortPrimaryCount" },
    { SectionType::GPUSortPrimaryScan, "GPUSortPrimaryScan" },
    { SectionType::GPUSortPrimarySort, "GPUSortPrimarySort" },
    { SectionType::GPUSortPrimaryCollapse, "GPUSortPrimaryCollapse" },
    { SectionType::ImageInfoPercentile, "ImageInfoPercentile" },
    { SectionType::ImageInfoError, "ImageInfoError" },
    { SectionType::ImageInfoHistogram, "ImageInfoHistogram" },
    { SectionType::ImageLoading, "ImageLoading" },
    { SectionType::PackedImageLoading, "PackedImageLoading" },
    { SectionType::BufferLoading, "BufferLoading" },
    { SectionType::BufferRequests, "BufferRequests" },
    { SectionType::BufferReleases, "BufferReleases" },
    { SectionType::FramebufferUpdate, "FramebufferUpdate" },
    { SectionType::AOVUpdate, "AOVUpdate" },
    { SectionType::TonemapUpdate, "TonemapUpdate" },
    { SectionType::FramebufferHostUpdate, "FramebufferHostUpdate" },
    { SectionType::AOVHostUpdate, "AOVHostUpdate" },
    { SectionType::Denoise, "Denoise" }
};

const char* Statistics::getShaderTypeName(ShaderType type)
{
    if (const auto it = ShaderTypeName.find(type); it != ShaderTypeName.end())
        return it->second;
    else
        return "Unknown";
}

const char* Statistics::getSectionTypeName(SectionType type)
{
    if (const auto it = SectionTypeName.find(type); it != SectionTypeName.end())
        return it->second;
    else
        return "Unknown";
}

class DumpTable {
public:
    DumpTable()
    {
    }

    void addRow(std::vector<std::string>&& cols)
    {
        mColumnCount = std::max(mColumnCount, cols.size());
        mData.emplace_back(std::move(cols));
    }

    std::string print(bool align_left = true, bool skip_empty = false, const std::string_view& delim = "|", size_t left_padding = 0)
    {
        if (mColumnCount == 0 || mData.empty())
            return {};

        std::vector<size_t> max_sizes(mColumnCount, 0);
        for (const auto& row : mData) {
            for (size_t col = 0; col < std::min(row.size(), mColumnCount); ++col)
                max_sizes[col] = std::max(max_sizes[col], row.at(col).length());
        }

        std::stringstream stream;
        for (const auto& row : mData) {
            for (size_t pad = 0; pad < left_padding; ++pad)
                stream << " ";

            const size_t max_cols = skip_empty ? std::min(mColumnCount, row.size()) : mColumnCount;
            for (size_t col = 0; col < max_cols; ++col) {
                const size_t max = max_sizes.at(col);
                const size_t len = col < row.size() ? row.at(col).length() : 0;

                if (col > 0)
                    stream << " ";

                if (!align_left && col != 0 /* TODO: Make this an option */) {
                    for (size_t i = len; i < max; ++i)
                        stream << " ";
                }

                if (col < row.size())
                    stream << row.at(col);

                if (align_left || col == 0) {
                    for (size_t i = len; i < max; ++i)
                        stream << " ";
                }

                if (col < max_cols - 1) {
                    stream << " ";
                    stream << delim;
                } else {
                    stream << std::endl;
                }
            }
        }

        return stream.str();
    }

private:
    size_t mColumnCount = 0;
    std::vector<std::vector<std::string>> mData;
};

std::string Statistics::dump(size_t totalMS, size_t iter, bool verbose, const std::string& suffixFirstRow) const
{
    DumpTable table;
    const auto dumpInline = [&](const std::string& name, size_t count, float elapsedMS, float percentage = -1, size_t workload = 0, size_t max_workload = 0, size_t min_workload = 0, bool skip_iter = false) {
        std::vector<std::string> cols;
        cols.emplace_back(name);

        {
            std::stringstream bstream;
            bstream << std::fixed << std::setprecision(2) << elapsedMS << " ms [" << count << "]";
            cols.emplace_back(bstream.str());
        }
        if (!skip_iter && iter != 0) {
            std::stringstream bstream;
            bstream << std::fixed << std::setprecision(2) << (elapsedMS / iter) << " ms [" << count / iter << "] per Iteration";
            cols.emplace_back(bstream.str());
        }
        if (count > 1) {
            std::stringstream bstream;
            bstream << std::fixed << std::setprecision(2) << (elapsedMS / count) << " ms per Call";
            cols.emplace_back(bstream.str());
        }
        if (percentage >= 0) {
            {
                std::stringstream bstream;
                bstream << std::fixed << std::setprecision(3) << percentage * 100 << "%";
                cols.emplace_back(bstream.str());
            }
            {
                std::stringstream bstream;
                bstream << "(min " << min_workload << ", avg " << std::fixed << std::setprecision(2) << (double(workload) / count) << ", max " << max_workload << ") per Call";
                cols.emplace_back(bstream.str());
            }
        }
        table.addRow(std::move(cols));
    };

    const auto dumpStats = [&](const std::string& name, const ShaderStats& stats) {
        dumpInline(name, stats.count, stats.elapsedMS);
    };

    const auto dumpStatsBasic = [&](const std::string& name, ShaderType type) {
        ShaderStats stats = mShaders.count(SmallShaderKey(type)) > 0 ? mShaders.at(SmallShaderKey(type)) : ShaderStats();
        dumpStats(name, stats);
    };

    const auto dumpStatsBasicVar = [&](const std::string& name, ShaderType type) {
        if (mShaders.count(SmallShaderKey(type)) > 0)
            dumpStats(name, mShaders.at(SmallShaderKey(type)));
    };

    const auto dumpStatsDetail = [&](const std::string& name, const ShaderStats& stats, size_t total_workload) {
        dumpInline(name, stats.count, stats.elapsedMS, static_cast<float>(double(stats.workload) / double(total_workload)), stats.workload, stats.maxWorkload, stats.minWorkload);
    };

    const auto dumpSectionStats = [&](const std::string& name, const SectionStats& stats, bool skipIter = true) {
        if (stats.count > 0)
            dumpInline(name, stats.count, stats.elapsedMS, -1, 0, 0, 0, skipIter);
    };

    const auto dumpQuantity = [=](size_t count) {
        std::stringstream bstream;
        bstream << count / totalMS << " per ms [" << count << "]";
        return bstream.str();
    };

    const auto sumMap = [&](ShaderType type) {
        ShaderStats a;
        for (const auto& pair : mShaders)
            if (pair.first.type() == type)
                a += pair.second;
        return a;
    };

    // Get all hits information
    const ShaderStats totalHits = sumMap(ShaderType::Hit);

    // Dump
    table.addRow({ "Statistics" + suffixFirstRow + ":" });
    table.addRow({ "  Shader:" });
    dumpStatsBasic("  |-Device", ShaderType::Device);
    dumpSectionStats("  ||-GPUSortPrimary", mSections[(size_t)SectionType::GPUSortPrimary], false);
    dumpSectionStats("  |||-GPUSortPrimaryReset", mSections[(size_t)SectionType::GPUSortPrimaryReset], false);
    dumpSectionStats("  |||-GPUSortPrimaryCount", mSections[(size_t)SectionType::GPUSortPrimaryCount], false);
    dumpSectionStats("  |||-GPUSortPrimaryScan", mSections[(size_t)SectionType::GPUSortPrimaryScan], false);
    dumpSectionStats("  |||-GPUSortPrimarySort", mSections[(size_t)SectionType::GPUSortPrimarySort], false);
    dumpSectionStats("  |||-GPUSortPrimaryCollapse", mSections[(size_t)SectionType::GPUSortPrimaryCollapse], false);
    dumpSectionStats("  ||-GPUSortSecondary", mSections[(size_t)SectionType::GPUSortSecondary], false);
    dumpSectionStats("  ||-GPUCompactPrimary", mSections[(size_t)SectionType::GPUCompactPrimary], false);
    dumpStatsBasic("  |-PrimaryTraversal", ShaderType::PrimaryTraversal);
    dumpStatsBasic("  |-SecondaryTraversal", ShaderType::SecondaryTraversal);
    dumpStatsBasic("  |-RayGeneration", ShaderType::RayGeneration);
    dumpStatsBasic("  |-Miss", ShaderType::Miss);
    dumpStats("  |-Hits", totalHits);

    if (verbose) {
        for (const auto& pair : mShaders) {
            if (pair.first.type() == ShaderType::Hit)
                dumpStatsDetail("  ||-@" + std::to_string(pair.first.subID()), pair.second, totalHits.workload);
        }
    }

    const ShaderStats totalAdvHits = sumMap(ShaderType::AdvancedShadowHit);
    const ShaderStats totalAdvMiss = sumMap(ShaderType::AdvancedShadowMiss);
    if (totalAdvHits.count > 0 || totalAdvMiss.count > 0) {
        table.addRow({ "  |-AdvancedShadow" });

        dumpStats("  ||-Miss", totalAdvHits);
        if (verbose) {
            for (const auto& pair : mShaders) {
                if (pair.first.type() == ShaderType::AdvancedShadowMiss)
                    dumpStatsDetail("  |||-@" + std::to_string(pair.first.subID()), pair.second, totalHits.workload);
            }
        }

        dumpStats("  ||-Hits", totalAdvMiss);
        if (verbose) {
            for (const auto& pair : mShaders) {
                if (pair.first.type() == ShaderType::AdvancedShadowHit)
                    dumpStatsDetail("  |||-@" + std::to_string(pair.first.subID()), pair.second, totalHits.workload);
            }
        }
    }

    const ShaderStats totalCallback = sumMap(ShaderType::Callback);
    if (totalCallback.count > 0) {
        dumpStats("  |-Callback", totalCallback);
        for (const auto& pair : mShaders) {
            if (pair.first.type() == ShaderType::Callback)
                dumpStats("  ||-@" + std::to_string(pair.first.subID()) + ">", pair.second);
        }
    }

    if (mShaders.count(SmallShaderKey(ShaderType::ImageInfo)) > 0) {
        dumpStatsBasic("  |-ImageInfo", ShaderType::ImageInfo);
        dumpSectionStats("  ||-ImageInfoPercentile", mSections[(size_t)SectionType::ImageInfoPercentile], false);
        dumpSectionStats("  ||-ImageInfoError", mSections[(size_t)SectionType::ImageInfoError], false);
        dumpSectionStats("  ||-ImageInfoHistogram", mSections[(size_t)SectionType::ImageInfoHistogram], false);
    }

    dumpStatsBasicVar("  |-Tonemap", ShaderType::Tonemap);
    dumpStatsBasicVar("  |-Glare", ShaderType::Glare);
    dumpStatsBasicVar("  |-Bake", ShaderType::Bake);

    table.addRow({ "  Sections:" });
    dumpSectionStats("  |-Denoise", mSections[(size_t)SectionType::Denoise]);
    dumpSectionStats("  |-ImageLoading", mSections[(size_t)SectionType::ImageLoading]);
    dumpSectionStats("  |-PackedImageLoading", mSections[(size_t)SectionType::PackedImageLoading]);
    dumpSectionStats("  |-BufferLoading", mSections[(size_t)SectionType::BufferLoading]);
    dumpSectionStats("  |-BufferRequests", mSections[(size_t)SectionType::BufferRequests]);
    dumpSectionStats("  |-BufferReleases", mSections[(size_t)SectionType::BufferReleases]);
    dumpSectionStats("  |-FramebufferUpdate", mSections[(size_t)SectionType::FramebufferUpdate]);
    dumpSectionStats("  |-AOVUpdate", mSections[(size_t)SectionType::AOVUpdate]);
    dumpSectionStats("  |-TonemapUpdate", mSections[(size_t)SectionType::TonemapUpdate]);
    dumpSectionStats("  |-FramebufferHostUpdate", mSections[(size_t)SectionType::FramebufferHostUpdate]);
    dumpSectionStats("  |-AOVHostUpdate", mSections[(size_t)SectionType::AOVHostUpdate]);

    table.addRow({ "  Quantities:" });
    table.addRow({ "  |-CameraRays", dumpQuantity(mQuantities[(size_t)Quantity::CameraRayCount]) });
    table.addRow({ "  |-ShadowRays", dumpQuantity(mQuantities[(size_t)Quantity::ShadowRayCount]) });
    table.addRow({ "  |-BounceRays", dumpQuantity(mQuantities[(size_t)Quantity::BounceRayCount]) });
    table.addRow({ "  |-PrimaryRays", dumpQuantity(mQuantities[(size_t)Quantity::CameraRayCount] + mQuantities[(size_t)Quantity::BounceRayCount]) });
    table.addRow({ "  |-TotalRays", dumpQuantity(mQuantities[(size_t)Quantity::CameraRayCount] + mQuantities[(size_t)Quantity::BounceRayCount] + mQuantities[(size_t)Quantity::ShadowRayCount]) });

    return table.print(false, true);
}

std::string Statistics::dumpAsJSON(float totalMS) const
{
    std::stringstream json;
    json << "{" << std::endl;

    json << "\"version\":[" << IG_VERSION_MAJOR << "," << IG_VERSION_MINOR << "," << IG_VERSION_PATCH << "]," << std::endl
         << "\"total_ms\":" << totalMS << "," << std::endl;

    // Dump timestamps
    json << "\"stream\": [" << std::endl;
    for (size_t k = 0; k < mStream.size(); ++k) {
        const auto& timestamp = mStream.at(k);

        json << "{";

        std::visit([&](auto&& type) {
            using T = std::decay_t<decltype(type)>;
            if constexpr (std::is_same_v<T, SmallShaderKey>) {
                json << "\"type\":" << (size_t)type.type() << ","
                     << "\"sub_id\":" << type.subID() << ",";
            } else if constexpr (std::is_same_v<T, SectionType>) {
                json << "\"type\":" << (size_t)type << ","
                     << "\"sub_id\":-1,";
            } else if constexpr (std::is_same_v<T, Statistics::Barrier>) {
                if (type == Statistics::Barrier::Frame)
                    json << "\"type\":-1,";
                else
                    json << "\"type\":-2,";

                json << "\"sub_id\":-1,";
            }
        },
                   timestamp.type);

        json << "\"start_ms\":" << timestamp.offsetStartMS << ","
             << "\"end_ms\":" << timestamp.offsetEndMS << ","
             << "\"workload\":" << timestamp.workload
             << "}";

        if (k + 1 < mStream.size())
            json << "," << std::endl;
    }
    json << "]," << std::endl;

    // Dump quantities
    json << "\"quantities\": [" << std::endl;
    for (size_t i = 0; i < mQuantities.size(); ++i) {
        json << "{"
             << "\"type\":" << i << ","
             << "\"value\":" << mQuantities.at(i)
             << "}";

        if (i + 1 < mQuantities.size())
            json << "," << std::endl;
    }
    json << "]," << std::endl;

    // Means
    json << "\"means\": [" << std::endl;
    for (const auto& p : mShaders) {
        json << "{"
             << "\"type\":" << (size_t)p.first.type() << ","
             << "\"sub_id\":" << p.first.subID() << ","
             << "\"count\":" << p.second.count << ","
             << "\"total_elapsed_ms\":" << p.second.elapsedMS << ","
             << "\"total_workload\":" << p.second.workload << ","
             << "\"min_workload\":" << p.second.minWorkload << ","
             << "\"max_workload\":" << p.second.maxWorkload
             << "}," << std::endl;
    }
    for (size_t i = 0; i < mSections.size(); ++i) {
        json << "{"
             << "\"type\":" << i << ","
             << "\"sub_id\":-1,"
             << "\"count\":" << mSections[i].count << ","
             << "\"total_elapsed_ms\":" << mSections[i].elapsedMS << ","
             << "\"total_workload\":0,"
             << "\"min_workload\":0,"
             << "\"max_workload\":0"
             << "}";

        if (i + 1 < mSections.size())
            json << "," << std::endl;
    }
    json << "]" << std::endl;

    json << "}" << std::endl;
    return json.str();
}

static inline Statistics::TimestampType getTimestampKey(int type, int subId)
{
    if (type < 0) {
        if (type == -1)
            return Statistics::Barrier::Frame;
        else
            return Statistics::Barrier::Iteration;
    } else {
        if (subId < 0) {
            return (SectionType)type;
        } else {
            return SmallShaderKey((ShaderType)type, (uint32)subId);
        }
    }
}

constexpr auto JsonFlags = rapidjson::kParseDefaultFlags | rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag | rapidjson::kParseNanAndInfFlag | rapidjson::kParseEscapedApostropheFlag;
bool Statistics::loadFromJSON(const std::string& jsonStr, float* pTotalMS)
{
    rapidjson::Document doc;
    if (doc.Parse<JsonFlags>(jsonStr.c_str()).HasParseError()) {
        IG_LOG(L_ERROR) << "JSON[" << doc.GetErrorOffset() << "]: " << rapidjson::GetParseError_En(doc.GetParseError()) << std::endl;
        return false;
    }

    if (!doc.IsObject()) {
        IG_LOG(L_ERROR) << "JSON: Expected root element to be an object." << std::endl;
        return false;
    }

    // Check version
    if (doc.HasMember("version")) {
        if (!doc["version"].IsArray()) {
            IG_LOG(L_ERROR) << "JSON: Expected 'version' element to be an array" << std::endl;
            return false;
        }

        const auto arr = doc["version"].GetArray();
        if (arr.Size() != 3) {
            IG_LOG(L_ERROR) << "JSON: Expected version element to be of size 3." << std::endl;
            return false;
        }

        if (arr[0].GetInt() != IG_VERSION_MAJOR || arr[1].GetInt() != IG_VERSION_MINOR || arr[2].GetInt() != IG_VERSION_PATCH) {
            IG_LOG(L_ERROR) << "JSON: Expected version to be "
                            << IG_VERSION_MAJOR << "." << IG_VERSION_MINOR << "." << IG_VERSION_PATCH << " but got "
                            << arr[0].GetInt() << "." << arr[1].GetInt() << "." << arr[2].GetInt() << std::endl;
            return false;
        }
    } else {
        IG_LOG(L_ERROR) << "JSON: Expected version element to be available." << std::endl;
        return false;
    }

    // Acquire total MS (might be useful for some instances)
    if (pTotalMS != nullptr) {
        if (doc.HasMember("total_ms") && doc["total_ms"].IsNumber()) {
            *pTotalMS = doc["total_ms"].GetFloat();
        }
    }

    // Get timestamps
    if (doc.HasMember("stream")) {
        if (!doc["stream"].IsArray()) {
            IG_LOG(L_ERROR) << "JSON: Expected 'streams' element to be an array" << std::endl;
            return false;
        }

        const auto stream = doc["stream"].GetArray();
        mStream.reserve(stream.Size());

        for (size_t k = 0; k < stream.Size(); ++k) {
            if (!stream[k].IsObject()) {
                IG_LOG(L_ERROR) << "JSON: Expected timestamp entries to be an object" << std::endl;
                return false;
            }

            const auto timestampObj = stream[k].GetObject();

            int type   = 0;
            int sub_id = -1;
            if (const auto it = timestampObj.FindMember("type"); it != timestampObj.MemberEnd()) {
                if (it->value.IsInt())
                    type = it->value.GetInt();
            }
            if (const auto it = timestampObj.FindMember("sub_id"); it != timestampObj.MemberEnd()) {
                if (it->value.IsInt())
                    sub_id = it->value.GetInt();
            }

            Timestamp timestamp = Timestamp{ getTimestampKey(type, sub_id) }; // Default

            if (const auto it = timestampObj.FindMember("start_ms"); it != timestampObj.MemberEnd()) {
                if (it->value.IsNumber())
                    timestamp.offsetStartMS = it->value.GetFloat();
            }
            if (const auto it = timestampObj.FindMember("end_ms"); it != timestampObj.MemberEnd()) {
                if (it->value.IsNumber())
                    timestamp.offsetEndMS = it->value.GetFloat();
            }
            if (const auto it = timestampObj.FindMember("workload"); it != timestampObj.MemberEnd()) {
                if (it->value.IsUint64())
                    timestamp.workload = it->value.GetUint64();
            }

            mStream.push_back(std::move(timestamp));
        }
    }

    // Get quantities
    if (doc.HasMember("quantities")) {
        if (!doc["quantities"].IsArray()) {
            IG_LOG(L_ERROR) << "JSON: Expected 'quantities' element to be an array" << std::endl;
            return false;
        }

        const auto arr = doc["quantities"].GetArray();
        for (size_t i = 0; i < arr.Size(); ++i) {
            if (!arr[i].IsObject()) {
                IG_LOG(L_ERROR) << "JSON: Expected quantity entries to be an object" << std::endl;
                return false;
            }

            const auto quantObj = arr[i].GetObject();

            uint32 type  = 0;
            uint64 value = 0;
            if (const auto it = quantObj.FindMember("type"); it != quantObj.MemberEnd()) {
                if (it->value.IsUint())
                    type = it->value.GetUint();
            }
            if (const auto it = quantObj.FindMember("value"); it != quantObj.MemberEnd()) {
                if (it->value.IsUint64())
                    value = it->value.GetUint64();
            }

            if (type >= mQuantities.size()) {
                IG_LOG(L_ERROR) << "JSON: Given quantity type is invalid" << std::endl;
                return false;
            }
            mQuantities[type] = value;
        }
    }

    // Get means
    if (doc.HasMember("means")) {
        if (!doc["means"].IsArray()) {
            IG_LOG(L_ERROR) << "JSON: Expected 'means' element to be an array" << std::endl;
            return false;
        }

        const auto arr = doc["means"].GetArray();
        for (size_t i = 0; i < arr.Size(); ++i) {
            if (!arr[i].IsObject()) {
                IG_LOG(L_ERROR) << "JSON: Expected mean entries to be an object" << std::endl;
                return false;
            }

            const auto meanObj = arr[i].GetObject();

            int type   = 0;
            int sub_id = -1;
            if (const auto it = meanObj.FindMember("type"); it != meanObj.MemberEnd()) {
                if (it->value.IsInt())
                    type = it->value.GetInt();
            }
            if (const auto it = meanObj.FindMember("sub_id"); it != meanObj.MemberEnd()) {
                if (it->value.IsInt())
                    sub_id = it->value.GetInt();
            }

            float totalElapsedMS = 0;
            size_t count         = 0;
            size_t totalWorkload = 0;
            size_t minWorkload   = 0;
            size_t maxWorkload   = 0;

            if (const auto it = meanObj.FindMember("total_elapsed_ms"); it != meanObj.MemberEnd()) {
                if (it->value.IsNumber())
                    totalElapsedMS = it->value.GetFloat();
            }
            if (const auto it = meanObj.FindMember("count"); it != meanObj.MemberEnd()) {
                if (it->value.IsUint64())
                    count = it->value.GetUint64();
            }
            if (const auto it = meanObj.FindMember("total_workload"); it != meanObj.MemberEnd()) {
                if (it->value.IsUint64())
                    totalWorkload = it->value.GetUint64();
            }
            if (const auto it = meanObj.FindMember("min_workload"); it != meanObj.MemberEnd()) {
                if (it->value.IsUint64())
                    minWorkload = it->value.GetUint64();
            }
            if (const auto it = meanObj.FindMember("max_workload"); it != meanObj.MemberEnd()) {
                if (it->value.IsUint64())
                    maxWorkload = it->value.GetUint64();
            }

            if (sub_id >= 0) {
                if (type >= (int)ShaderType::_COUNT) {
                    IG_LOG(L_ERROR) << "JSON: Given mean type is invalid" << std::endl;
                    return false;
                }

                auto& stats       = mShaders.try_emplace(SmallShaderKey((ShaderType)type, sub_id)).first->second;
                stats.count       = count;
                stats.elapsedMS   = totalElapsedMS;
                stats.workload    = totalWorkload;
                stats.minWorkload = minWorkload;
                stats.maxWorkload = maxWorkload;
            } else {
                if (type >= (int)SectionType::_COUNT) {
                    IG_LOG(L_ERROR) << "JSON: Given mean type is invalid" << std::endl;
                    return false;
                }

                auto& stats     = mSections[type];
                stats.count     = count;
                stats.elapsedMS = totalElapsedMS;
            }
        }
    }

    finalizeStream();
    return true;
}
} // namespace IG