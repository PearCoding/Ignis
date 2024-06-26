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

void loaderThread(RenderWidgetInternal* internal, Path scene_file);

constexpr uint32 FisheyeWidth  = 512;
constexpr uint32 FisheyeHeight = 512;

class RenderWidgetInternal {
public:
    inline RenderWidgetInternal()
        : mTexture(nullptr)
        , mWidth(0)
        , mHeight(0)
        , mFOV(60)
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
            // std::cout << width << "x" << height << std::endl;
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

                mLoading       = true;
                mLoadingThread = std::make_unique<std::thread>(loaderThread, this, scene_file);
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

            if (ImGui::Begin("Render")) {
                if (mRuntime->currentIterationCount() > 0) {
                    if (mTexture) {
                        if (!mPerspectivePass->run()) {
                            IG_LOG(L_FATAL) << "Failed to run perspective pass" << std::endl;
                        } else {
                            // TODO
                            IG_ASSERT(mBuffer.size() >= mWidth * mHeight, "Invalid buffer");
                            mPerspectivePass->copyOutputToHost("_perspective_output", mBuffer.data(), mWidth * mHeight * sizeof(uint32));
                            // mRuntime->tonemap(buf, TonemapSettings{ "", (size_t)0, false, 1.0f, 1.0f, 0.0f });

                            SDL_UpdateTexture(mTexture, nullptr, mBuffer.data(), static_cast<int>(mWidth * sizeof(uint32)));
                            ImGui::Image((void*)mTexture, ImVec2((float)mWidth, (float)mHeight));
                        }
                    }
                }

                const ImVec2 contentMin  = ImGui::GetWindowContentRegionMin();
                const ImVec2 contentMax  = ImGui::GetWindowContentRegionMax();
                const ImVec2 contentSize = ImVec2(contentMax.x - contentMin.x, contentMax.y - contentMin.y);
                if (!mTexture || mWidth != contentSize.x || mHeight != contentSize.y)
                    onContentResize((size_t)contentSize.x, (size_t)contentSize.y);
            }
            ImGui::End();
        }
    }

    void onInput()
    {
    }

    inline void loadFromFile(const Path& path)
    {
        auto options              = RuntimeOptions::makeDefault();
        options.IsInteractive     = true;
        options.EnableTonemapping = true;
        options.OverrideFilmSize  = { FisheyeWidth, FisheyeHeight };

        SceneParser parser;
        auto scene = parser.loadFromFile(path);

        auto prevCamera = scene->camera();

        auto cameraObject = std::make_shared<SceneObject>(SceneObject::OT_CAMERA, "fisheye", Path{});
        cameraObject->setProperty("mode", SceneProperty::fromString("circular"));
        if (prevCamera && prevCamera->hasProperty("transform"))
            cameraObject->setProperty("transform", prevCamera->property("transform"));
        if (prevCamera && prevCamera->hasProperty("fov")) // TODO: Other types?
            mFOV = prevCamera->property("fov").getNumber(mFOV);

        scene->setCamera(cameraObject);

        mRuntime = std::make_unique<Runtime>(options);
        if (mRuntime->loadFromScene(scene.get())) {
            std::stringstream shader;
            for (int i = 0; ig_shader[i]; ++i)
                shader << ig_shader[i] << std::endl;

            auto loaderOptions     = mRuntime->loaderOptions();
            loaderOptions.FilePath = path;
            loaderOptions.Scene    = scene.get();
            shader << PerspectiveShader::generate(loaderOptions);

            mPerspectivePass = mRuntime->createPass(shader.str());
            if (!mPerspectivePass)
                IG_LOG(L_ERROR) << "Could not compile the shader code" << std::endl;

            mPerspectivePass->setParameter("_perspective_fov", mFOV);
        }

        mLoading = false;
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
    std::vector<uint32> mBuffer;
    size_t mWidth, mHeight;
    float mFOV;

    std::unique_ptr<Runtime> mRuntime;
    std::shared_ptr<RenderPass> mPerspectivePass;
    Path mRequestedPath;

    std::unique_ptr<std::thread> mLoadingThread;
    std::atomic<bool> mLoading;
};

void loaderThread(RenderWidgetInternal* internal, Path scene_file)
{
    internal->loadFromFile(scene_file);
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