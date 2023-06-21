#pragma once

#include <unordered_map>
#include <string>

#include "SmallShaderKey.h"
#include "StatisticTypes.h"
#include "Timer.h"

namespace IG {

class IG_LIB Statistics {
public:
    struct ShaderStats {
        float elapsedMS     = 0;
        size_t count        = 0;
        size_t workload     = 0; // This might overflow, but who cares for statistical stuff after that huge number of iterations
        size_t max_workload = 0;
        size_t min_workload = std::numeric_limits<size_t>::max();

        ShaderStats& operator+=(const ShaderStats& other);
    };

    struct SectionStats {
        float elapsedMS = 0;
        size_t count    = 0;

        SectionStats& operator+=(const SectionStats& other);
    };

    Statistics();

    inline void reset()
    {
        *this = Statistics();
    }

    void add(ShaderType type, size_t id, const ShaderStats& stats);
    void add(SectionType type, const SectionStats& stats);
    void add(Quantity quantity, uint64 value);
    void add(const Statistics& other);

    void sync(ShaderType type, size_t id, float ms);
    void sync(SectionType type, float ms);

    [[nodiscard]] std::string dump(size_t totalMS, size_t iter, bool verbose, const std::string& suffixFirstRow = {}) const;

private:
    std::unordered_map<SmallShaderKey, ShaderStats, SmallShaderKeyHash> mShaders;
    std::array<uint64, (size_t)Quantity::_COUNT> mQuantities;
    std::array<SectionStats, (size_t)SectionType::_COUNT> mSections;
};
} // namespace IG