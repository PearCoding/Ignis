#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct LoaderEntity {
    static bool load(LoaderContext& ctx, LoaderResult& res);
};
} // namespace IG