#include "ExternalProcess.h"
#include "Logger.h"

#ifdef IG_OS_LINUX
#error Process implementation missing
#elif defined(IG_OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <codecvt>
#include <locale>
#else
#error Process implementation missing
#endif

namespace IG {

#ifdef IG_OS_LINUX
class ExternalProcessInternal {
};
#elif defined(IG_OS_WINDOWS)

static inline std::wstring s2ws(const std::string& str)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.from_bytes(str);
}

class ExternalProcessInternal {
public:
    const Path mPath;
    const std::vector<std::string> mParameters;

    PROCESS_INFORMATION pi;

    // HANDLE stdOutWr = INVALID_HANDLE_VALUE;
    HANDLE stdOutRd = INVALID_HANDLE_VALUE;
    HANDLE stdInWr  = INVALID_HANDLE_VALUE;
    // HANDLE stdInRd  = INVALID_HANDLE_VALUE;

    inline ExternalProcessInternal(const Path& exe, const std::vector<std::string>& parameters)
        : mPath(exe)
        , mParameters(parameters)
    {
        pi.hProcess = INVALID_HANDLE_VALUE;
        pi.hThread  = INVALID_HANDLE_VALUE;
    }

    inline ~ExternalProcessInternal()
    {
        if (pi.hProcess != INVALID_HANDLE_VALUE)
            CloseHandle(pi.hProcess);

        if (pi.hThread != INVALID_HANDLE_VALUE)
            CloseHandle(pi.hThread);

        // if (stdOutWr != INVALID_HANDLE_VALUE)
        //     CloseHandle(stdOutWr);

        if (stdOutRd != INVALID_HANDLE_VALUE)
            CloseHandle(stdOutRd);

        if (stdInWr != INVALID_HANDLE_VALUE)
            CloseHandle(stdInWr);

        // if (stdInRd != INVALID_HANDLE_VALUE)
        //     CloseHandle(stdInRd);
    }

    inline bool start()
    {
        // Setup commandline
        std::wstringstream cmdLineStream;
        cmdLineStream << mPath << L" ";
        for (const auto& str : mParameters)
            cmdLineStream << s2ws(str) << L" ";
        std::wstring cmdLine = cmdLineStream.str();

        // Setup pipes
        SECURITY_ATTRIBUTES saAttr;

        HANDLE stdOutWr = INVALID_HANDLE_VALUE;
        HANDLE stdInRd  = INVALID_HANDLE_VALUE;

        ZeroMemory(&saAttr, sizeof(saAttr));
        saAttr.nLength              = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle       = TRUE;
        saAttr.lpSecurityDescriptor = NULL;
        if (!CreatePipe(&stdOutRd, &stdOutWr, &saAttr, 0)) {
            IG_LOG(L_ERROR) << "CreatePipe failed: " << std::system_category().message(GetLastError()) << std::endl;
            return false;
        }

        if (!SetHandleInformation(stdOutRd, HANDLE_FLAG_INHERIT, 0)) {
            IG_LOG(L_ERROR) << "SetHandleInformation failed: " << std::system_category().message(GetLastError()) << std::endl;
            return false;
        }

        if (!CreatePipe(&stdInRd, &stdInWr, &saAttr, 0)) {
            IG_LOG(L_ERROR) << "CreatePipe failed: " << std::system_category().message(GetLastError()) << std::endl;
            return false;
        }

        if (!SetHandleInformation(stdInWr, HANDLE_FLAG_INHERIT, 0)) {
            IG_LOG(L_ERROR) << "SetHandleInformation failed: " << std::system_category().message(GetLastError()) << std::endl;
            return false;
        }

        // Setup process
        STARTUPINFOW si;
        ZeroMemory(&si, sizeof(si));
        si.cb         = sizeof(si);
        si.hStdError  = stdOutWr;
        si.hStdOutput = stdOutWr;
        si.hStdInput  = stdInRd;
        si.dwFlags |= STARTF_USESTDHANDLES;

        ZeroMemory(&pi, sizeof(pi));

        // Start the child process.
        if (!CreateProcessW(NULL,           // No module name (use command line)
                            cmdLine.data(), // Command line
                            NULL,           // Process handle not inheritable
                            NULL,           // Thread handle not inheritable
                            TRUE,           // Set handle inheritance to TRUE
                            0,              // No creation flags
                            NULL,           // Use parent's environment block
                            NULL,           // Use parent's starting directory
                            &si,            // Pointer to STARTUPINFO structure
                            &pi)            // Pointer to PROCESS_INFORMATION structure
        ) {
            IG_LOG(L_ERROR) << "CreateProcess failed: " << std::system_category().message(GetLastError()) << std::endl;
            return false;
        }

        // The following handles are not needed
        if (pi.hThread != INVALID_HANDLE_VALUE) {
            CloseHandle(pi.hThread);
            pi.hThread = INVALID_HANDLE_VALUE;
        }

        if (stdOutWr != INVALID_HANDLE_VALUE)
            CloseHandle(stdOutWr);

        if (stdInRd != INVALID_HANDLE_VALUE)
            CloseHandle(stdInRd);

        return true;
    }

    inline int exitCode() const
    {
        DWORD exit_code;
        if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
            IG_LOG(L_ERROR) << "GetExitCodeProcess failed: " << std::system_category().message(GetLastError()) << std::endl;
            return -1;
        }
        return exit_code;
    }

    inline bool isRunning() const
    {
        return exitCode() == STILL_ACTIVE;
    }

    inline void waitForInit()
    {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(20ms);
        // if (WaitForInputIdle(pi.hProcess, INFINITE) == WAIT_FAILED)
        //     IG_LOG(L_ERROR) << "WaitForInputIdle failed: " << std::system_category().message(GetLastError()) << std::endl;
    }

    inline void waitForFinish()
    {
        if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
            IG_LOG(L_ERROR) << "WaitForSingleObject failed: " << std::system_category().message(GetLastError()) << std::endl;
    }

    inline void sendOnce(const std::string& data)
    {
        size_t written = 0;
        while (written < data.size()) {
            DWORD dwWrite   = (DWORD)(data.size() - written);
            DWORD dwWritten = 0;
            if (!WriteFile(stdInWr, data.data() + written, dwWrite, &dwWritten, NULL)) {
                IG_LOG(L_ERROR) << "WriteFile failed: " << std::system_category().message(GetLastError()) << std::endl;
                break;
            }
            written += dwWritten;
        }

        // Close to signalize pipe finished!
        if (stdInWr != INVALID_HANDLE_VALUE)
            CloseHandle(stdInWr);
        stdInWr = INVALID_HANDLE_VALUE;
    }

    inline std::string receiveOnce()
    {
        constexpr size_t BUFSIZE = 1024;
        CHAR chBuf[BUFSIZE];

        std::string output;
        while (true) {
            DWORD dwRead;
            BOOL bSuccess = ReadFile(stdOutRd, chBuf, BUFSIZE, &dwRead, NULL);
            if (!bSuccess || dwRead == 0)
                break;

            output += std::string(chBuf, chBuf + dwRead);
        }

        if (stdOutRd != INVALID_HANDLE_VALUE)
            CloseHandle(stdOutRd);
        stdOutRd = INVALID_HANDLE_VALUE;

        return output;
    }
};
#endif

// -----------------------------------------------------------------------------------

ExternalProcess::ExternalProcess()
{
}

ExternalProcess::ExternalProcess(const Path& exe, const std::vector<std::string>& parameters)
    : mInternal(new ExternalProcessInternal(exe, parameters))
{
}

ExternalProcess::~ExternalProcess()
{
}

bool ExternalProcess::start()
{
    if (mInternal)
        return mInternal->start();

    return false;
}

int ExternalProcess::exitCode() const
{
    return mInternal ? mInternal->exitCode() : 0;
}

bool ExternalProcess::isRunning() const
{
    return mInternal ? mInternal->isRunning() : false;
}

void ExternalProcess::waitForInit()
{
    if (!isRunning())
        return;

    mInternal->waitForInit();
}

void ExternalProcess::waitForFinish()
{
    if (!isRunning())
        return;

    mInternal->waitForFinish();
}

void ExternalProcess::sendOnce(const std::string& data)
{
    if (mInternal)
        mInternal->sendOnce(data);
}

std::string ExternalProcess::receiveOnce()
{
    return mInternal ? mInternal->receiveOnce() : std::string{};
}
} // namespace IG