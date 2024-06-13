#pragma once

#include "technique/TechniqueVariant.h"

namespace IG {
    class ScriptCompiler;

class ShaderManager {
public:
    struct ShaderEntry {
        const ShaderOutput<std::string>* Source;
        std::string Function;
        ShaderOutput<void*>* Compiled;
    };

    inline void add(const std::string& id, const ShaderEntry& entry)
    {
        mEntries[id] = entry;
    }

    [[nodiscard]] bool compile(ScriptCompiler* compiler, size_t threads);

private:
    std::unordered_map<std::string, ShaderEntry> mEntries;
};

} // namespace IG