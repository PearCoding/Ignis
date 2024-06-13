#pragma once

#include "IG_Config.h"

namespace IG {
class ExternalProcess {
public:
    ExternalProcess();
    ExternalProcess(const std::string& name, const Path& exe, const std::vector<std::string>& parameters);
    ~ExternalProcess();

    [[nodiscard]] bool start();

    [[nodiscard]] int exitCode() const;
    [[nodiscard]] bool isRunning() const;

    void waitForInit();
    void waitForFinish();

    // Both functions can only be used once!
    bool sendOnce(const std::string& data);
    std::string receiveOnce();

private:
    std::unique_ptr<class ExternalProcessInternal> mInternal;
};
} // namespace IG