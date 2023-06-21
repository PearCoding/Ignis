#include "Statistics.h"
#include "Logger.h"

#include <iomanip>
#include <sstream>

namespace IG {
Statistics::Statistics()
    : mQuantities()
{
    std::fill(mQuantities.begin(), mQuantities.end(), 0);
}

void Statistics::add(ShaderType type, size_t id, const ShaderStats& stats)
{
    const auto& p = mShaders.try_emplace(SmallShaderKey(type, id), stats);
    if (!p.second)
        p.first->second += stats;
}

void Statistics::add(SectionType type, const SectionStats& stats)
{
    mSections[(size_t)type] += stats;
}

void Statistics::add(Quantity quantity, uint64 value)
{
    mQuantities[(size_t)quantity] += value;
}

void Statistics::sync(ShaderType type, size_t id, float ms)
{
    mShaders[SmallShaderKey(type, id)].elapsedMS = ms;
}

void Statistics::sync(SectionType type, float ms)
{
    mSections[(size_t)type].elapsedMS = ms;
}

Statistics::ShaderStats& Statistics::ShaderStats::operator+=(const Statistics::ShaderStats& other)
{
    elapsedMS += other.elapsedMS;
    count += other.count;
    workload += other.workload;
    max_workload = std::max(max_workload, other.max_workload);
    min_workload = std::min(min_workload, other.min_workload);

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
        dumpInline(name, stats.count, stats.elapsedMS, static_cast<float>(double(stats.workload) / double(total_workload)), stats.workload, stats.max_workload, stats.min_workload);
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

} // namespace IG