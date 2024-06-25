#include "RenderPass.h"
#include "Runtime.h"

namespace IG {
RenderPass::RenderPass(Runtime* runtime, void* callback)
    : mRuntime(runtime)
    , mCallback(callback)
    , mRegistry(std::make_shared<ParameterSet>())
{
}

RenderPass::~RenderPass()
{
}

bool RenderPass::run()
{
    return mRuntime->runPass(*this);
}

size_t RenderPass::getOutputSizeInBytes(const std::string& name) const
{
    return mRuntime->getBufferSizeInBytes(name);
}

bool RenderPass::copyOutputToHost(const std::string& name, void* dst, size_t maxSizeInBytes)
{
    return mRuntime->copyBufferToHost(name, dst, maxSizeInBytes);
}
} // namespace IG