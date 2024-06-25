#include "RenderWidget.h"
#include "Logger.h"
#include "PerspectiveShader.h"
#include "Runtime.h"

#include "imgui.h"
#include <SDL.h>

#include <thread>

extern const char* ig_shader[];

namespace IG {
extern SDL_Renderer* sRenderer;

void loaderThread(RenderWidgetInternal* internal, Path scene_file, RuntimeOptions options);

constexpr uint32 FisheyeWidth  = 256;
constexpr uint32 FisheyeHeight = 128;

class RenderWidgetInternal {
public:
    inline RenderWidgetInternal()
        : mTexture(nullptr)
        , mWidth(0)
        , mHeight(0)
        , mLoading(false)
    {
    }

    inline ~RenderWidgetInternal()
    {
        if (mLoadingThread) {
            mLoading = false;
            mLoadingThread->join();
        }

        if (mTexture)
            SDL_DestroyTexture(mTexture);
    }

    void openFile(const Path& path)
    {
        if (!mLoading)
            mRequestedPath = path;
    }

    void onWindowResize(size_t width, size_t height)
    {
        // Nothing
        IG_UNUSED(width, height);
    }

    void onContentResize(size_t width, size_t height)
    {
        mWidth  = width;
        mHeight = height;

        setupTextureBuffer(width, height);

        if (mPerspectivePass) {
            mPerspectivePass->setParameter("_output_width", (int)mWidth);
            mPerspectivePass->setParameter("_output_height", (int)mHeight);
        }
    }

    void onRender()
    {
        // Some modal stuff for io
        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();

        if (!mLoading && !mRequestedPath.empty()) {
            bool setupRuntime = !mRuntime;
            if (mRuntime)
                ImGui::OpenPopup("Close previous scene?");
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("Close previous scene?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Do you want to close the previous scene?");
                ImGui::Separator();

                if (ImGui::Button("Yes", ImVec2(100, 0))) {
                    setupRuntime = true;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                    mRequestedPath.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (!mRequestedPath.empty() && setupRuntime) {
                const Path scene_file = mRequestedPath;
                mRequestedPath.clear();

                mRuntime.reset();

                auto options              = RuntimeOptions::makeDefault();
                options.IsInteractive     = true;
                options.EnableTonemapping = true;
                options.OverrideCamera    = "fishlens";
                options.OverrideFilmSize  = { FisheyeWidth, FisheyeHeight };

                mLoading       = true;
                mLoadingThread = std::make_unique<std::thread>(loaderThread, this, scene_file, options);
            }
        }

        if (mLoading) {
            ImGui::OpenPopup("Loading scene");
        } else {
            if (mLoadingThread) {
                mLoadingThread->join();
                mLoadingThread.reset();
            }
        }

        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        bool popupOpen = mLoading;
        if (ImGui::BeginPopupModal("Loading scene", &popupOpen, ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Please wait, loading scene...");
            ImGui::EndPopup();
        }

        if (!mLoading && mRuntime) {
            mRuntime->step();

            ImGui::SetNextWindowContentSize(ImVec2(128, 128));
            if (ImGui::Begin("Render")) {
                if (mRuntime->currentIterationCount() > 0) {
                    if (mTexture) {
                        if (!mPerspectivePass->run()) {
                            IG_LOG(L_FATAL) << "Failed to run perspective pass" << std::endl;
                        } else {
                            // TODO
                            uint32* buf = mBuffer.data();
                            // mRuntime->tonemap(buf, TonemapSettings{ "", (size_t)0, false, 1.0f, 1.0f, 0.0f });

                            SDL_UpdateTexture(mTexture, nullptr, buf, static_cast<int>(mWidth * sizeof(uint32_t)));
                            ImGui::Image((void*)mTexture, ImVec2((float)mRuntime->framebufferWidth(), (float)mRuntime->framebufferHeight()));
                        }
                    }
                }
            }
            const ImVec2 windowSize = ImGui::GetWindowSize();
            if (!mTexture || mWidth != windowSize.x || mHeight != windowSize.y)
                onContentResize(windowSize.x, windowSize.y);
            ImGui::End();
        }
    }

    void onInput()
    {
    }

    inline void assignRuntime(std::shared_ptr<RenderPass>&& perspectivePass, std::unique_ptr<Runtime>&& runtime)
    {
        mRuntime         = std::move(runtime);
        mPerspectivePass = std::move(perspectivePass);
        mLoading         = false;
    }

private:
    // Buffer stuff
    bool setupTextureBuffer(size_t width, size_t height)
    {
        if (mTexture)
            SDL_DestroyTexture(mTexture);

        mTexture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (int)width, (int)height);
        if (!mTexture) {
            IG_LOG(L_FATAL) << "Cannot create SDL texture: " << SDL_GetError() << std::endl;
            return false;
        }

        mBuffer.resize(width * height);
        return true;
    }

    SDL_Texture* mTexture = nullptr;
    std::vector<uint32_t> mBuffer;
    size_t mWidth, mHeight;

    std::unique_ptr<Runtime> mRuntime;
    std::shared_ptr<RenderPass> mPerspectivePass;
    Path mRequestedPath;

    std::unique_ptr<std::thread> mLoadingThread;
    std::atomic<bool> mLoading;
};

void loaderThread(RenderWidgetInternal* internal, Path scene_file, RuntimeOptions options)
{
    auto runtime = std::make_unique<Runtime>(options);
    if (!runtime->loadFromFile(scene_file)) {
        internal->assignRuntime(nullptr, nullptr);
    } else {
        std::stringstream shader;
        for (int i = 0; ig_shader[i]; ++i)
            shader << ig_shader[i] << std::endl;

        shader << PerspectiveShader::generate(runtime->loaderOptions());

        auto pass = runtime->createPass(shader.str());
        if (!pass) {
            IG_LOG(L_ERROR) << "Could not compile the shader code" << std::endl;
            internal->assignRuntime(nullptr, nullptr);
        } else {
            internal->assignRuntime(std::move(pass), std::move(runtime));
        }
    }
}

RenderWidget::RenderWidget()
    : mInternal(std::make_unique<RenderWidgetInternal>())
{
}

RenderWidget::~RenderWidget()
{
}

void RenderWidget::openFile(const Path& path)
{
    mInternal->openFile(path);
}

void RenderWidget::onWindowResize(Widget*, size_t width, size_t height)
{
    mInternal->onWindowResize(width, height);
}

void RenderWidget::onRender(Widget*)
{
    mInternal->onRender();
}

void RenderWidget::onInput(Widget*)
{
    mInternal->onInput();
}

}; // namespace IG