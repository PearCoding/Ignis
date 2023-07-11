#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <variant>

#include "SmallShaderKey.h"
#include "StatisticTypes.h"
#include "Timer.h"

namespace IG {

class IG_LIB Statistics {
private:
    struct ShaderStats {
        float elapsedMS    = 0;
        size_t count       = 0;
        size_t workload    = 0; // This might overflow, but who cares for statistical stuff after that huge number of iterations
        size_t maxWorkload = 0;
        size_t minWorkload = std::numeric_limits<size_t>::max();

        ShaderStats& operator+=(const ShaderStats& other);
    };

    struct SectionStats {
        float elapsedMS = 0;
        size_t count    = 0;

        SectionStats& operator+=(const SectionStats& other);
    };

    using ShaderMap = std::unordered_map<SmallShaderKey, ShaderStats, SmallShaderKeyHash>;

public:
    using TimestampType = std::variant<SmallShaderKey, SectionType>;
    struct Timestamp {
        TimestampType type;
        float offsetStartMS = 0; // Total start offset from iteration start in milliseconds
        float offsetEndMS   = 0; // Total end offset from iteration start in milliseconds
        size_t workload     = 0;
        bool dirty          = true; // If false, it is already consumed
    };

    struct MeanEntry {
        float totalElapsedMS = 0;
        size_t count         = 0;
        size_t totalWorkload = 0; // This might overflow, but who cares for statistical stuff after that huge number of iterations
        size_t maxWorkload   = 0;
        size_t minWorkload   = std::numeric_limits<size_t>::max();
    };

    Statistics();

    inline void reset()
    {
        *this = Statistics();
    }

    void nextStream();

    void record(const Timestamp& timestamp);

    const std::vector<Timestamp>& lastStream() const { return mStreams[(mCurrentStream - 1) % mStreams.size()]; }

    void add(Quantity quantity, uint64 value);
    void add(const Statistics& other);

    class ShaderIterator : public std::iterator<std::forward_iterator_tag, const SmallShaderKey> {
    private:
        friend class Statistics;

        ShaderMap::const_iterator mIt;
        explicit ShaderIterator(const ShaderMap::const_iterator& it)
            : mIt(it)
        {
        }

    public:
        inline ShaderIterator& operator++()
        {
            mIt++;
            return *this;
        }
        inline ShaderIterator operator++(int)
        {
            ShaderIterator retval = *this;
            ++(*this);
            return retval;
        }

        inline bool operator==(ShaderIterator other) const { return mIt == other.mIt; }
        inline bool operator!=(ShaderIterator other) const { return !(*this == other); }
        inline reference operator*() const { return mIt->first; }
        inline pointer operator->() const { return &mIt->first; }
    };

    [[nodiscard]] inline ShaderIterator shadersBegin() const { return ShaderIterator(mShaders.begin()); }
    [[nodiscard]] inline ShaderIterator shadersEnd() const { return ShaderIterator(mShaders.end()); }

    [[nodiscard]] MeanEntry entry(ShaderType type, uint32 sub_id = 0) const;
    [[nodiscard]] MeanEntry entry(SectionType type) const;

    [[nodiscard]] inline uint64 quantity(Quantity type) const { return mQuantities.at((size_t)type); }

    [[nodiscard]] std::string dump(size_t totalMS, size_t iter, bool verbose, const std::string& suffixFirstRow = {}) const;
    [[nodiscard]] std::string dumpAsJSON(float totalMS) const;
    bool loadFromJSON(const std::string& jsonStr, float* pTotalMS);

    /// @brief Consume current stream and fillup the average informations.
    void consume();

    [[nodiscard]] static const char* getShaderTypeName(ShaderType type);
    [[nodiscard]] static const char* getSectionTypeName(SectionType type);

private:
    std::vector<Timestamp>& currentStream() { return mStreams[mCurrentStream]; }
    std::array<std::vector<Timestamp>, 2> mStreams;
    size_t mCurrentStream;

    ShaderMap mShaders;
    std::array<uint64, (size_t)Quantity::_COUNT> mQuantities;
    std::array<SectionStats, (size_t)SectionType::_COUNT> mSections;
};
} // namespace IG