#pragma once

#include "LogListener.h"
#include <fstream>

namespace IG {
class ConsoleLogListener : public LogListener {
public:
    ConsoleLogListener(bool useAnsi = true);
    virtual ~ConsoleLogListener() = default;

    virtual void startEntry(LogLevel level) override;
    virtual void writeEntry(char c) override;

    inline void enableAnsi(bool b = true) { mUseAnsi = b; }
    inline bool isUsingAnsi() const { return mUseAnsi; }

    /// Close any additional console started due to the console subsystem (only relevant on Windows)
    void closeExtraConsole();
    /// Open a new console if none exists. Currently only supported on Windows
    void openConsole();

private:
    bool mUseAnsi;

    void startColoring(LogLevel level);
    void stopColoring();
};
} // namespace IG
