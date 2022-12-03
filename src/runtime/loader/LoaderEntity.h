#pragma once

#include "LoaderContext.h"

namespace IG {
class LoaderEntity {
public:
    void prepare(const LoaderContext& ctx);
    bool load(LoaderContext& ctx);

    [[nodiscard]] inline size_t entityCount() const { return mEntityCount; }

private:
    size_t mEntityCount = 0;
};
} // namespace IG