#include "ScriptCompiler.h"
#include "Logger.h"
#include "RuntimeInfo.h"
#include "device/ICompilerDevice.h"

#include <fstream>
#include <regex>

// Will be populated by api_collector
extern const char* ig_api[];
extern const char* ig_api_paths[];

namespace IG {
ScriptCompiler::ScriptCompiler(const std::shared_ptr<ICompilerDevice>& compiler, const Path& stdlibDir)
    : mCompiler(compiler)
    , mStdLibPath(stdlibDir)
    , mOptimizationLevel(3)
    , mVerbose(false)
{
}

ScriptCompiler::~ScriptCompiler()
{
}

void* ScriptCompiler::compile(const std::string& script, const std::string& function) const
{
    std::lock_guard<std::mutex> _guard(mCompileMutex);

    return mCompiler->compileAndGet(
        ICompilerDevice::Settings{
            .OptimizationLevel = (int)mOptimizationLevel,
            .Verbose           = mVerbose },
        script, function);
}

static inline std::optional<std::string_view> getFromAPI(const std::string_view& path)
{
    for (int i = 0; ig_api_paths[i]; ++i) {
        if (ig_api_paths[i] == path)
            return ig_api[i];
    }
    return std::nullopt;
}

std::string ScriptCompiler::prepare(const std::string& script) const
{

    // TODO
    std::stringstream source;

    if (mStdLibPath.empty()) {
        for (int i = 0; ig_api[i]; ++i)
            source << ig_api[i];
    } else {
        source << mStdLibPath;
    }

    source << std::endl;
    source << script;

    return source.str();
}

std::string ScriptCompiler::prepare(const std::string_view& script, const std::string_view& path, std::unordered_set<std::string>& included_paths) const
{
    static const std::regex include_statement = std::regex(R"(\/\/\#\<\s*include\s*\=\s*\"([^\"]*)\"\s*\>)", std::regex::ECMAScript);

    std::string copy = (std::string)script;
    while (true) {
        std::smatch include_match;
        if (std::regex_search(copy, include_match, include_statement)) {
            const std::string include_path = std::string(include_match[1].first, include_match[1].second);
            if (included_paths.contains(include_path)) {
                // Already included
                copy.replace(include_match[0].first, include_match[0].second, "");
            } else if (mStdLibPath.empty()) {
                // Use internal API
                if (const auto val = getFromAPI(include_path); val) {
                    included_paths.insert(include_path);
                    const std::string result = prepare((std::string)val.value(), include_path, included_paths);
                    copy.replace(include_match[0].first, include_match[0].second, result);
                } else {
                    IG_LOG(L_ERROR) << "Could not resolve API include '" << include_path << "' in '" << path << "'" << std::endl;
                    return {};
                }
            } else {
                // Use external files
                const auto full_path = mStdLibPath / include_path;
                if (std::filesystem::exists(full_path)) {
                    std::string content;
                    try {
                        std::ifstream f(full_path);
                        content = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                    } catch (const std::exception& e) {
                        IG_LOG(L_ERROR) << "Could not find include " << full_path << " in '" << path << "': " << e.what() << std::endl;
                        return {};
                    }

                    included_paths.insert(include_path);
                    const std::string result = prepare(content, include_path, included_paths);
                    copy.replace(include_match[0].first, include_match[0].second, result);
                } else {
                    IG_LOG(L_ERROR) << "Could not find include " << full_path << " in '" << path << "'" << std::endl;
                    return {};
                }
            }

            std::cout << include_match[0] << std::endl;
            std::cout << include_match[1] << std::endl;
        } else {
            break;
        }
    }
}
} // namespace IG