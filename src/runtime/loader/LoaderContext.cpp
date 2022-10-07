#include "LoaderContext.h"
#include "Image.h"
#include "Logger.h"
#include "serialization/VectorSerializer.h"
#include "table/SceneDatabase.h"

namespace IG {

using namespace Parser;

std::filesystem::path LoaderContext::handlePath(const std::filesystem::path& path, const Parser::Object& obj) const
{
    if (path.is_absolute())
        return path;

    if (!obj.baseDir().empty()) {
        const auto p = obj.baseDir() / path;
        if (std::filesystem::exists(p))
            return std::filesystem::canonical(p);
    }

    if (!FilePath.empty()) {
        const auto p = FilePath.parent_path() / path;
        if (std::filesystem::exists(p))
            return std::filesystem::canonical(p);
    }

    return std::filesystem::canonical(path);
}

} // namespace IG