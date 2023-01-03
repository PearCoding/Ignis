#pragma once

#include "CameraProxy.h"

namespace IG {
struct CameraPose {
    Vector3f Eye = Vector3f(0, 0, 0);
    Vector3f Dir = Vector3f(0, 0, 1);
    Vector3f Up  = Vector3f(0, 1, 0);

    inline CameraPose() = default;
    inline explicit CameraPose(const CameraProxy& cam)
        : Eye(cam.Eye)
        , Dir(cam.Direction)
        , Up(cam.Up)
    {
    }
};

class PoseManager {
public:
    inline CameraPose pose(size_t i) const { return mPoses[i % mPoses.size()]; }
    void setPose(size_t i, const CameraPose& pose);

    void load(const std::filesystem::path& file);
    void save(const std::filesystem::path& file) const;

    inline void setInitalPose(const CameraPose& pose) { mInitialPose = pose; }
    inline CameraPose initialPose() const { return mInitialPose; }

    inline size_t poseCount() const { return mPoses.size(); }

private:
    CameraPose mInitialPose;
    std::array<CameraPose, 10> mPoses;
};
} // namespace IG