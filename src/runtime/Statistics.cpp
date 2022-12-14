#include "Statistics.h"
#include <sstream>

namespace IG {
Statistics::Statistics()
    : mQuantities()
{
    std::fill(mQuantities.begin(), mQuantities.end(), 0);
}

void Statistics::beginShaderLaunch(ShaderType type, size_t workload, size_t id)
{
    ShaderStats* stats = getStats(type, id);
    stats->timer.start();
    stats->count++;
    stats->workload += workload;
    stats->max_workload = std::max(stats->max_workload, workload);
    stats->min_workload = std::min(stats->min_workload, workload);
}

void Statistics::endShaderLaunch(ShaderType type, size_t id)
{
    ShaderStats* stats = getStats(type, id);
    stats->elapsedMS += stats->timer.stopMS();
}

void Statistics::beginSection(SectionType type)
{
    SectionStats& stats = mSections[(size_t)type];
    stats.timer.start();
    stats.count++;
}

void Statistics::endSection(SectionType type)
{
    SectionStats& stats = mSections[(size_t)type];
    stats.elapsedMS += stats.timer.stopMS();
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
    mDeviceStats += other.mDeviceStats;
    mRayGenerationStats += other.mRayGenerationStats;
    mMissStats += other.mMissStats;
    for (const auto& pair : other.mHitStats)
        mHitStats[pair.first] += pair.second;
    for (const auto& pair : other.mAdvancedShadowHitStats)
        mAdvancedShadowHitStats[pair.first] += pair.second;
    for (const auto& pair : other.mAdvancedShadowMissStats)
        mAdvancedShadowMissStats[pair.first] += pair.second;
    for (const auto& pair : other.mCallbackStats)
        mCallbackStats[pair.first] += pair.second;
    mTonemapStats += other.mTonemapStats;
    mImageInfoStats += other.mImageInfoStats;

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

std::string Statistics::dump(size_t totalMS, size_t iter, bool verbose) const
{
    DumpTable table;
    const auto dumpInline = [&](const std::string& name, size_t count, size_t elapsedMS, float percentage = -1, size_t max_workload = 0, size_t min_workload = 0, bool skip_iter = false) {
        std::vector<std::string> cols;
        cols.emplace_back(name);

        {
            std::stringstream bstream;
            bstream << elapsedMS << "ms [" << count << "]";
            cols.emplace_back(bstream.str());
        }
        if (!skip_iter && iter != 0) {
            std::stringstream bstream;
            bstream << elapsedMS / iter << "ms [" << count / iter << "] per Iteration";
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
                bstream << "(min " << min_workload << ", max " << max_workload << ") per call";
                cols.emplace_back(bstream.str());
            }
        }
        table.addRow(std::move(cols));
    };

    const auto dumpStats = [&](const std::string& name, const ShaderStats& stats) {
        dumpInline(name, stats.count, stats.elapsedMS);
    };

    const auto dumpStatsDetail = [&](const std::string& name, const ShaderStats& stats, size_t total_workload) {
        dumpInline(name, stats.count, stats.elapsedMS, static_cast<float>(double(stats.workload) / double(total_workload)), stats.max_workload, stats.min_workload);
    };

    const auto dumpSectionStats = [&](const std::string& name, const SectionStats& stats) {
        if (stats.count > 0)
            dumpInline(name, stats.count, stats.elapsedMS, -1, 0, 0, true);
    };

    const auto dumpQuantity = [=](size_t count) {
        std::stringstream bstream;
        bstream << count / totalMS << " per ms [" << count << "]";
        return bstream.str();
    };

    const auto sumMap = [](const std::map<size_t, ShaderStats>& map) {
        ShaderStats a;
        for (const auto& pair : map)
            a += pair.second;
        return a;
    };

    // Get all hits information
    ShaderStats totalHits = sumMap(mHitStats);

    // Dump
    table.addRow({ "Statistics:" });
    table.addRow({ "  Shader:" });
    dumpStats("  |-Device", mDeviceStats);
    dumpStats("  |-PrimaryTraversal", mPrimaryTraversalStats);
    dumpStats("  |-SecondaryTraversal", mSecondaryTraversalStats);
    dumpStats("  |-RayGeneration", mRayGenerationStats);
    dumpStats("  |-Miss", mMissStats);
    dumpStats("  |-Hits", totalHits);

    if (verbose) {
        for (const auto& pair : mHitStats)
            dumpStatsDetail("  ||-@" + std::to_string(pair.first), pair.second, totalHits.workload);
    }

    if (!mAdvancedShadowHitStats.empty() || !mAdvancedShadowMissStats.empty()) {
        ShaderStats totalAdvHits = sumMap(mAdvancedShadowHitStats);
        ShaderStats totalAdvMiss = sumMap(mAdvancedShadowMissStats);

        table.addRow({ "  |-AdvancedShadow" });

        dumpStats("  ||-Miss", totalAdvHits);
        if (verbose) {
            for (const auto& pair : mAdvancedShadowMissStats)
                dumpStatsDetail("  |||-@" + std::to_string(pair.first), pair.second, totalHits.workload);
        }

        dumpStats("  ||-Hits", totalAdvMiss);
        if (verbose) {
            for (const auto& pair : mAdvancedShadowHitStats)
                dumpStatsDetail("  |||-@" + std::to_string(pair.first), pair.second, totalHits.workload);
        }
    }

    if (!mCallbackStats.empty()) {
        table.addRow({ "  |-Callback" });
        for (const auto& pair : mCallbackStats)
            dumpStats("  ||-@" + std::to_string(pair.first) + ">", pair.second);
    }

    if (mImageInfoStats.count > 0)
        dumpStats("  |-ImageInfo", mImageInfoStats);

    if (mTonemapStats.count > 0)
        dumpStats("  |-Tonemap", mTonemapStats);

    if (mBakeStats.count > 0)
        dumpStats("  |-Bake", mBakeStats);

    table.addRow({ "  Sections:" });
    dumpSectionStats("  |-ImageLoading", mSections[(size_t)SectionType::ImageLoading]);
    dumpSectionStats("  |-PackedImageLoading", mSections[(size_t)SectionType::PackedImageLoading]);
    dumpSectionStats("  |-BufferLoading", mSections[(size_t)SectionType::BufferLoading]);
    dumpSectionStats("  |-BufferRequests", mSections[(size_t)SectionType::BufferRequests]);
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

Statistics::ShaderStats* Statistics::getStats(ShaderType type, size_t id)
{
    switch (type) {
    default:
    case ShaderType::Device:
        return &mDeviceStats;
    case ShaderType::PrimaryTraversal:
        return &mPrimaryTraversalStats;
    case ShaderType::SecondaryTraversal:
        return &mSecondaryTraversalStats;
    case ShaderType::RayGeneration:
        return &mRayGenerationStats;
    case ShaderType::Miss:
        return &mMissStats;
    case ShaderType::Hit:
        return &mHitStats[id];
    case ShaderType::AdvancedShadowHit:
        return &mAdvancedShadowHitStats[id];
    case ShaderType::AdvancedShadowMiss:
        return &mAdvancedShadowMissStats[id];
    case ShaderType::Callback:
        return &mHitStats[id];
    case ShaderType::Tonemap:
        return &mTonemapStats;
    case ShaderType::ImageInfo:
        return &mImageInfoStats;
    case ShaderType::Bake:
        return &mBakeStats;
    }
}
} // namespace IG