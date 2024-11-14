#include "ExternalProcess.h"
#include "Logger.h"
#include "RuntimeInfo.h"
#include "StringUtils.h"

#include <thread>

#ifdef IG_OS_LINUX
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fstream>

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
    const Path exePath;
    const std::vector<std::string> cmdParameters;
    const Path logFile;

    pid_t pid;
    mutable int exit_code;

    int stdIn[2];

    inline ExternalProcessInternal(const std::string& name, const Path& exe, const std::vector<std::string>& parameters, const Path& logFile)
        : exePath(exe)
        , cmdParameters(parameters)
        , logFile(logFile)
        , pid(-1)
        , exit_code(-1)
        , stdIn{ InvalidPipe, InvalidPipe }
    {
        IG_UNUSED(name);
    }

    inline ~ExternalProcessInternal()
    {
        if (stdIn[PipeWrite] != InvalidPipe)
            close(stdIn[PipeWrite]);

        // Remove empty logs
        if (std::filesystem::file_size(logFile) == 0)
            std::filesystem::remove(logFile);
    }

    inline bool start()
    {
        // Init pipes
        if (pipe(stdIn) < 0) {
            IG_LOG(L_ERROR) << "Initializing stdin for process " << exePath << " failed: " << std::strerror(errno) << std::endl;
            return false;
        }

        int tmpOut = open(logFile.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (tmpOut < 0) {
            IG_LOG(L_ERROR) << "Getting file handle for temporary file for process " << exePath << " failed: " << std::strerror(errno) << std::endl;
            return false;
        }

        // Prepare for child
        const std::string path  = exePath.string();
        const char** parameters = new const char*[cmdParameters.size() + 2];

        parameters[0] = path.c_str();
        for (size_t i = 0; i < cmdParameters.size(); ++i)
            parameters[i + 1] = cmdParameters[i].c_str();
        parameters[cmdParameters.size() + 1] = nullptr;

        // Init process
        pid = fork();

        if (pid == 0) {
            // -> Child process

            // Redirect stdin
            if (dup2(stdIn[PipeRead], STDIN_FILENO) == -1)
                IG_LOG(L_FATAL) << "dup2 for stdin of process failed: " << std::strerror(errno) << std::endl;

            // // Redirect stdout
            if (dup2(tmpOut, STDOUT_FILENO) == -1)
                IG_LOG(L_FATAL) << "dup2 for stdout of process failed: " << std::strerror(errno) << std::endl;

            // Redirect stderr
            if (dup2(tmpOut, STDERR_FILENO) == -1)
                IG_LOG(L_FATAL) << "dup2 for stderr of process failed: " << std::strerror(errno) << std::endl;

            // all these are for use by parent only
            close(stdIn[PipeRead]);
            close(stdIn[PipeWrite]);
            close(tmpOut);

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
            close(tmpOut);

            // Check for error
            if (pid == -1) {
                IG_LOG(L_ERROR) << "Fork of process " << exePath << " failed: " << std::strerror(errno) << std::endl;
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
                IG_LOG(L_ERROR) << "waitpid for " << exePath << " (" << pid << " | " << logFile << ") failed: " << std::strerror(errno) << std::endl;
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
            IG_LOG(L_ERROR) << "waitpid for " << exePath << " (" << pid << " | " << logFile << ") failed: " << std::strerror(errno) << std::endl;
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
                IG_LOG(L_ERROR) << "write for " << exePath << " (" << pid << " | " << logFile << ") failed: " << std::strerror(errno) << std::endl;
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

        try {
            std::stringstream stream;
            stream << std::ifstream(logFile).rdbuf();
            return stream.str();
        } catch (...) {
            return {};
        }
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
    const Path exePath;
    const std::vector<std::string> cmdParameters;
    const Path logFile;

    PROCESS_INFORMATION pi;

    HANDLE stdInWr    = INVALID_HANDLE_VALUE;
    HANDLE stdOutFile = INVALID_HANDLE_VALUE;

    inline ExternalProcessInternal(const std::string& name, const Path& exe, const std::vector<std::string>& parameters, const Path& logFile)
        : exePath(exe)
        , cmdParameters(parameters)
        , logFile(logFile)
    {
        IG_UNUSED(name);

        pi.hProcess = INVALID_HANDLE_VALUE;
        pi.hThread  = INVALID_HANDLE_VALUE;
    }

    inline ~ExternalProcessInternal()
    {
        if (pi.hProcess != INVALID_HANDLE_VALUE)
            CloseHandle(pi.hProcess);

        if (pi.hThread != INVALID_HANDLE_VALUE)
            CloseHandle(pi.hThread);

        if (stdOutFile != INVALID_HANDLE_VALUE)
            CloseHandle(stdOutFile);

        if (stdInWr != INVALID_HANDLE_VALUE)
            CloseHandle(stdInWr);

        // Remove empty logs
        if (std::filesystem::file_size(logFile) == 0)
            std::filesystem::remove(logFile);
    }

    inline bool start()
    {
        // Setup commandline
        std::wstringstream cmdLineStream;
        cmdLineStream << exePath << L" ";
        for (const auto& str : cmdParameters)
            cmdLineStream << s2ws(str) << L" ";
        std::wstring cmdLine = cmdLineStream.str();

        // Setup security stuff
        SECURITY_ATTRIBUTES saAttr;

        ZeroMemory(&saAttr, sizeof(saAttr));
        saAttr.nLength              = sizeof(saAttr);
        saAttr.bInheritHandle       = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        // Direct output to file
        stdOutFile = CreateFileW(logFile.c_str(),
                                 FILE_GENERIC_WRITE | FILE_GENERIC_READ,
                                 FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
                                 &saAttr,
                                 OPEN_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL);

        if (stdOutFile == INVALID_HANDLE_VALUE) {
            IG_LOG(L_ERROR) << "CreateFileW failed: " << std::system_category().message(GetLastError()) << std::endl;
            return false;
        }

        // Setup pipes
        HANDLE stdInRd = INVALID_HANDLE_VALUE;
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
        si.hStdError  = stdOutFile;
        si.hStdOutput = stdOutFile;
        si.hStdInput  = stdInRd;
        si.dwFlags |= STARTF_USESTDHANDLES;

        ZeroMemory(&pi, sizeof(pi));

        // Start the child process.
        if (!CreateProcessW(exePath.native().data(),                    // Module name (use command line)
                            cmdLine.data(),                             // Command line
                            NULL,                                       // Process handle not inheritable
                            NULL,                                       // Thread handle not inheritable
                            TRUE,                                       // Set handle inheritance to TRUE
                            CREATE_NO_WINDOW | INHERIT_PARENT_AFFINITY, // Only keep the affinity and do not create a new console window if parent is closed
                            NULL,                                       // Use parent's environment block
                            NULL,                                       // Use parent's starting directory
                            &si,                                        // Pointer to STARTUPINFO structure
                            &pi)                                        // Pointer to PROCESS_INFORMATION structure
        ) {
            IG_LOG(L_ERROR) << "CreateProcess failed: " << std::system_category().message(GetLastError()) << std::endl;
            return false;
        }

        // The following handles are not needed
        if (pi.hThread != INVALID_HANDLE_VALUE) {
            CloseHandle(pi.hThread);
            pi.hThread = INVALID_HANDLE_VALUE;
        }

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
        // using namespace std::chrono_literals;
        // std::this_thread::sleep_for(20ms);
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
            BOOL bSuccess = ReadFile(stdOutFile, chBuf, BUFSIZE, &dwRead, NULL);
            if (!bSuccess || dwRead == 0)
                break;

            output += std::string(chBuf, chBuf + dwRead);
        }

        if (stdOutFile != INVALID_HANDLE_VALUE)
            CloseHandle(stdOutFile);
        stdOutFile = INVALID_HANDLE_VALUE;

        return output;
    }
};
#endif

// -----------------------------------------------------------------------------------

ExternalProcess::ExternalProcess(const std::string& name, const Path& exe, const std::vector<std::string>& parameters, const Path& logFile)
    : mInternal(new ExternalProcessInternal(name, exe, parameters, logFile))
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