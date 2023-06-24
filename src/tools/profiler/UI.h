#pragma once

#include "IG_Config.h"

#include <memory>

namespace IG {
class Statistics;
class UI {
public:
    UI(const Statistics& stats, float total_ms);
    ~UI();

    enum class InputResult {
        Continue, // Continue, nothing of importance changed
        Quit      // Quit the application
    };
    [[nodiscard]] InputResult handleInput();

    void update();

private:
    friend class UIInternal;
    std::unique_ptr<class UIInternal> mInternal;
};
} // namespace IG