#include "StatisticHandler.h"

namespace IG {
StatisticHandler::StatisticHandler(const anydsl::Device& device)
    : mDevice(device)
    , mStatistics()
{
    for (size_t i = 0; i < (size_t)SectionType::_COUNT; ++i)
        mSections.emplace_back(mDevice);
}

void StatisticHandler::beginShaderLaunch(ShaderType type, size_t workload, size_t id)
{
    mShaders.try_emplace(SmallShaderKey(type, id), mDevice).first->second.recordStart();
    mStatistics.add(type, id, Statistics::ShaderStats{ 0 /*Later*/, 1, workload, workload, workload });
}

void StatisticHandler::endShaderLaunch(ShaderType type, size_t id)
{
    mShaders.try_emplace(SmallShaderKey(type, id), mDevice).first->second.recordStop();
    // Elapsed will be updated in the finalize method
}

void StatisticHandler::beginSection(SectionType type)
{
    mSections[(size_t)type].recordStart();
    mStatistics.add(type, Statistics::SectionStats{ 0 /*Later*/, 1 });
}

void StatisticHandler::endSection(SectionType type)
{
    mSections[(size_t)type].recordStop();
    // Elapsed will be updated in the finalize method
}

void StatisticHandler::finalize()
{
    for (auto& p : mShaders) {
        p.second.tryFinalize();
        mStatistics.sync(p.first.type(), p.first.subID(), p.second.elapsedMS());
    }

    for (size_t i = 0; i < (size_t)SectionType::_COUNT; ++i) {
        mSections[i].tryFinalize();
        mStatistics.sync((SectionType)i, mSections[i].elapsedMS());
    }
}
} // namespace IG