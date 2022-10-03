#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
class LoaderEntity {
public:
    void prepare(const LoaderContext& ctx);
    bool load(LoaderContext& ctx, LoaderResult& result);

    [[nodiscard]] inline size_t entityCount() const { return mEntityCount; }

private:
    size_t mEntityCount = 0;
};
} // namespace IG