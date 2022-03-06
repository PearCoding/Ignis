#pragma once

#include "LogListener.h"
#include <fstream>

namespace IG {
class ConsoleLogListener : public LogListener {
public:
    ConsoleLogListener(bool useAnsi = true);
    virtual ~ConsoleLogListener();

    virtual void startEntry(LogLevel level) override;
    virtual void writeEntry(char c) override;

    inline void enableAnsi(bool b = true) { mUseAnsi = b; }
    inline bool isUsingAnsi() const { return mUseAnsi; }

private:
    bool mUseAnsi;
};
} // namespace IG
