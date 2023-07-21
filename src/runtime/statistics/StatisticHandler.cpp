#include "StatisticHandler.h"

namespace IG {
StatisticHandler::StatisticHandler(const anydsl::Device& device)
    : mDevice(device)
    , mStarted(false)
    , mStartEvent(device)
    , mFrameEvent(device)
    , mFrameSubmitted(true)
    , mIterationEvent(device)
    , mIterationSubmitted(true)
    , mStatistics()
    , mRecordingStream(false)
{
    for (size_t i = 0; i < (size_t)SectionType::_COUNT; ++i)
        mSections.emplace_back(mDevice);
}

void StatisticHandler::startIteration()
{
    if (!mStarted) {
        mStartEvent.record();
        mStarted = true;
    }

    finalize();
    mIterationEvent.record();
    mIterationSubmitted = false;
}

void StatisticHandler::startFrame()
{
    if (!mStarted) {
        mStartEvent.record();
        mStarted = true;
    }

    finalize();
    mFrameEvent.record();
    mFrameSubmitted = false;
}

void StatisticHandler::beginShaderLaunch(ShaderType type, size_t workload, size_t id)
{
    const auto key = SmallShaderKey(type, id);
    auto& s        = mShaders.try_emplace(key, mDevice).first->second;
    if (s.state() == DeviceTimer::State::Ended) {
        pushToStream(key);
        s.finalize();
    }

    mShaderPayload.try_emplace(key, workload).first->second = workload;
    s.recordStart();
}

void StatisticHandler::endShaderLaunch(ShaderType type, size_t id)
{
    mShaders.try_emplace(SmallShaderKey(type, id), mDevice).first->second.recordStop();
}

void StatisticHandler::beginSection(SectionType type)
{
    auto& s = mSections[(size_t)type];
    if (s.state() == DeviceTimer::State::Ended) {
        pushToStream(type);
        s.finalize();
    }

    s.recordStart();
}

void StatisticHandler::endSection(SectionType type)
{
    mSections[(size_t)type].recordStop();
}

static inline bool checkIfReady(const anydsl::Event& ev)
{
    AnyDSLResult res = anydslQueryEvent(ev.handle(), AnyDSL_NULL_HANDLE, nullptr);
    return res == AnyDSL_SUCCESS;
}

void StatisticHandler::pushToStream(const SmallShaderKey& key)
{
    const auto& s   = mShaders.at(key);
    size_t workload = mShaderPayload.at(key);

    IG_ASSERT(checkIfReady(s.start()) && checkIfReady(s.end()), "Expected event section to be ready");

    const float start = anydsl::Event::elapsedTimeMS(mStartEvent, s.start());
    const float end   = anydsl::Event::elapsedTimeMS(mStartEvent, s.end());

    mStatistics.record(Statistics::Timestamp{ key, start, end, workload }, mRecordingStream);
}

void StatisticHandler::pushToStream(const SectionType& key)
{
    const auto& s = mSections.at((size_t)key);

    IG_ASSERT(checkIfReady(s.start()) && checkIfReady(s.end()), "Expected event section to be ready");

    const float start = anydsl::Event::elapsedTimeMS(mStartEvent, s.start());
    const float end   = anydsl::Event::elapsedTimeMS(mStartEvent, s.end());
    mStatistics.record(Statistics::Timestamp{ key, start, end, 0 }, mRecordingStream);
}

void StatisticHandler::finalize()
{
    for (auto& p : mShaders) {
        if (p.second.state() == DeviceTimer::State::Ended) {
            pushToStream(p.first);
            p.second.finalize();
        }
    }

    for (size_t i = 0; i < (size_t)SectionType::_COUNT; ++i) {
        if (mSections[i].state() == DeviceTimer::State::Ended) {
            pushToStream((SectionType)i);
            mSections[i].finalize();
        }
    }

    if (!mFrameSubmitted) {
        const float pointInTimeFrame = anydsl::Event::elapsedTimeMS(mStartEvent, mFrameEvent);
        if (pointInTimeFrame >= 0) {
            mStatistics.record(Statistics::Timestamp{ Statistics::Barrier::Frame, pointInTimeFrame, pointInTimeFrame, 0 }, mRecordingStream);
            mFrameSubmitted = true;
        }
    }

    if (!mIterationSubmitted) {
        const float pointInTimeIteration = anydsl::Event::elapsedTimeMS(mStartEvent, mIterationEvent);
        if (pointInTimeIteration >= 0) {
            mStatistics.record(Statistics::Timestamp{ Statistics::Barrier::Iteration, pointInTimeIteration, pointInTimeIteration, 0 }, mRecordingStream);
            mIterationSubmitted = true;
        }
    }
}
} // namespace IG