#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
class LoaderEntity {
public:
    void prepare(const LoaderContext& ctx);
    bool load(LoaderContext& ctx, LoaderResult& result);
};
} // namespace IG