#pragma once

#include "IG_Config.h"

namespace IG {
class SHA256 {
public:
    SHA256();

    void update(const uint8* message, size_t len);
    void update(const std::string_view& str);

    [[nodiscard]] std::string final();

protected:
    void transform(const uint8* message, size_t block_nb);

    size_t mTotalLength;
    size_t mCurrentLength;
    std::array<uint8, 512 / 4> mBlock;
    std::array<uint32, 8> mState;
};
} // namespace IG