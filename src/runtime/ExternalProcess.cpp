#include "ExternalProcess.h"
#include "Logger.h"

#include <thread>

#ifdef IG_OS_LINUX
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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
constexpr int InvalidPipe = -1;
constexpr int PipeRead    = 0;
constexpr int PipeWrite   = 1;

class ExternalProcessInternal {
public:
    const Path mPath;
    const std::vector<std::string> mParameters;

    pid_t pid;
    mutable int exit_code;

    int stdIn[2];
    int stdOut[2];

    inline ExternalProcessInternal(const Path& exe, const std::vector<std::string>& parameters)
        : mPath(exe)
        , mParameters(parameters)
        , pid(-1)
        , exit_code(-1)
        , stdIn{ InvalidPipe, InvalidPipe }
        , stdOut{ InvalidPipe, InvalidPipe }
    {
    }

    inline ~ExternalProcessInternal()
    {
        if (stdIn[PipeWrite] != InvalidPipe)
            close(stdIn[PipeWrite]);

        if (stdOut[PipeRead] != InvalidPipe)
            close(stdOut[PipeRead]);
    }

    inline bool start()
    {
        // Init pipes

        if (pipe(stdIn) < 0) {
            IG_LOG(L_ERROR) << "Initializing stdin for process " << mPath << " failed: " << std::strerror(errno) << std::endl;
            return false;
        }

        if (pipe(stdOut) < 0) {
            IG_LOG(L_ERROR) << "Initializing stdout for process " << mPath << " failed: " << std::strerror(errno) << std::endl;
            return false;
        }

        // Prepare for child
        const std::string path  = mPath.string();
        const char** parameters = new const char*[mParameters.size() + 2];

        parameters[0] = path.c_str();
        for (size_t i = 0; i < mParameters.size(); ++i)
            parameters[i + 1] = mParameters[i].c_str();
        parameters[mParameters.size() + 1] = nullptr;

        // Init process
        pid = fork();

        if (pid == 0) {
            // -> Child process

            // Redirect stdin
            if (dup2(stdIn[PipeRead], STDIN_FILENO) == -1)
                IG_LOG(L_FATAL) << "dup2 for stdin of process failed: " << std::strerror(errno) << std::endl;

            // Redirect stdout
            if (dup2(stdOut[PipeWrite], STDOUT_FILENO) == -1)
                IG_LOG(L_FATAL) << "dup2 for stdout of process failed: " << std::strerror(errno) << std::endl;

            // Redirect stderr
            if (dup2(stdOut[PipeWrite], STDERR_FILENO) == -1)
                IG_LOG(L_FATAL) << "dup2 for stderr of process failed: " << std::strerror(errno) << std::endl;

            // all these are for use by parent only
            close(stdIn[PipeRead]);
            close(stdIn[PipeWrite]);
            close(stdOut[PipeRead]);
            close(stdOut[PipeWrite]);

            if (execv(parameters[0], (char**)parameters) == -1)
                IG_LOG(L_FATAL) << "fork/exec of process failed: " << std::strerror(errno) << std::endl;

            std::exit(-1);

#if IG_CC_MSC
            __assume(false);
#else // GCC, Clang
            __builtin_unreachable();
#endif
        } else {
            // -> Parent process
            delete[] parameters;

            // Close unnecessary handles
            close(stdIn[PipeRead]);
            stdIn[PipeRead] = InvalidPipe;
            close(stdOut[PipeWrite]);
            stdOut[PipeWrite] = InvalidPipe;

            // Check for error
            if (pid == -1) {
                IG_LOG(L_ERROR) << "Fork of process " << mPath << " failed: " << std::strerror(errno) << std::endl;
                return false;
            }
        }

        return true;
    }

    inline int exitCode() const
    {
        return exit_code;
    }

    inline bool isRunning() const
    {
        if (pid == -1)
            return false;

        int status;
        int result = waitpid(pid, &status, WNOHANG);

        if (result < 0) {
            if (errno != ECHILD)
                IG_LOG(L_ERROR) << "waitpid for " << mPath << " (" << pid << ") failed: " << std::strerror(errno) << std::endl;
            return false;
        } else {
            if (result == 0) {
                return true;
            } else {
                if (WIFEXITED(status))
                    exit_code = WEXITSTATUS(status);
                return false;
            }
        }
    }

    inline void waitForInit()
    {
        // using namespace std::chrono_literals;
        // std::this_thread::sleep_for(20ms);
    }

    inline void waitForFinish()
    {
        if (pid == -1)
            return;

        int status;
        if (waitpid(pid, &status, 0) < 0) {
            IG_LOG(L_ERROR) << "waitpid for " << mPath << " (" << pid << ") failed: " << std::strerror(errno) << std::endl;
            return;
        }

        if (WIFEXITED(status))
            exit_code = WEXITSTATUS(status);
    }

    inline bool sendOnce(const std::string& data)
    {
        if (pid == -1)
            return false;

        size_t written = 0;
        while (written < data.size()) {
            size_t toWrite = data.size() - written;
            int result     = write(stdIn[PipeWrite], data.c_str() + written, toWrite);
            if (result < 0) {
                IG_LOG(L_ERROR) << "write for " << mPath << " (" << pid << ") failed: " << std::strerror(errno) << std::endl;
                break;
            }
            written += (size_t)result;
        }

        close(stdIn[PipeWrite]);
        stdIn[PipeWrite] = InvalidPipe;

        return written >= data.size();
    }

    inline std::string receiveOnce()
    {
        if (pid == -1)
            return {};

        std::string output;
        while (true) {
            char c;
            int result = read(stdOut[PipeRead], &c, 1);
            if (result < 0) {
                IG_LOG(L_ERROR) << "read for " << mPath << " (" << pid << ") failed: " << std::strerror(errno) << std::endl;
                break;
            } else if (result == 0) {
                // EOF
                break;
            }

            output += c;
        }

        close(stdOut[PipeRead]);
        stdOut[PipeRead] = InvalidPipe;
        return output;
    }
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

    inline bool sendOnce(const std::string& data)
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

        return written >= data.size();
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

bool ExternalProcess::sendOnce(const std::string& data)
{
    if (mInternal)
        return mInternal->sendOnce(data);
    return false;
}

std::string ExternalProcess::receiveOnce()
{
    return mInternal ? mInternal->receiveOnce() : std::string{};
}
} // namespace IG