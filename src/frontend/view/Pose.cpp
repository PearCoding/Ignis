#include "Pose.h"

#include <algorithm>
#include <fstream>

namespace IG {
void PoseManager::setPose(size_t i, const CameraPose& pose)
{
    mPoses[i % mPoses.size()] = pose;
}

static CameraPose read_pose(std::istream& in)
{
    CameraPose pose;
    in >> pose.Eye[0] >> pose.Eye[1] >> pose.Eye[2];
    in >> pose.Dir[0] >> pose.Dir[1] >> pose.Dir[2];
    in >> pose.Up[0] >> pose.Up[1] >> pose.Up[2];
    return pose;
}

static void write_pose(const CameraPose& pose, std::ostream& out)
{
    out << pose.Eye[0] << " " << pose.Eye[1] << " " << pose.Eye[2];
    out << " " << pose.Dir[0] << " " << pose.Dir[1] << " " << pose.Dir[2];
    out << " " << pose.Up[0] << " " << pose.Up[1] << " " << pose.Up[2];
}

void PoseManager::load(const Path& file)
{
    // Reset poses
    std::generate(mPoses.begin(), mPoses.end(), []() { return CameraPose(); });

    // Check if pose exists
    if (!std::filesystem::exists(file))
        return;

    // Read all poses
    std::ifstream stream(file);
    for (size_t i = 0; i < mPoses.size() && stream.good(); ++i) {
        mPoses[i] = read_pose(stream);
    }
}

void PoseManager::save(const Path& file) const
{
    std::ofstream stream(file);
    for (const auto& pose : mPoses) {
        write_pose(pose, stream);
        stream << std::endl;
    }
}

} // namespace IG