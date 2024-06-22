#include "RenderPass.h"
#include "Runtime.h"

namespace IG {
RenderPass::RenderPass(Runtime* runtime, void* callback)
    : mRuntime(runtime)
    , mCallback(callback)
{
}

RenderPass::~RenderPass()
{
}

bool RenderPass::run()
{
    return mRuntime->runPass(*this);
}
} // namespace IG