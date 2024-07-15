#include "RenderWidget.h"
#include "CameraProxy.h"
#include "ColorbarGizmo.h"
#include "Logger.h"
#include "Runtime.h"
#include "ShaderGenerator.h"

#include "UI.h"

#include <thread>

extern const char* ig_shader[];

namespace IG {
extern SDL_Renderer* sRenderer;
extern int sMainWindowDockID;

void loaderThread(RenderWidgetInternal* internal, Path scene_file);

constexpr float RSPEED  = 0.005f;
constexpr float KRSPEED = 10 * RSPEED;

class RenderWidgetInternal {
public:
    inline RenderWidgetInternal()
        : mTexture(nullptr)
        , mWidth(0)
        , mHeight(0)
        , mCurrentParameters(RenderWidget::Parameters{
              .FOV               = 60,
              .ExposureFactor    = 0,
              .ExposureOffset    = 0,
              .ToneMappingMethod = RenderWidget::ToneMappingMethod::PbrNeutral,
              .OverlayMethod     = RenderWidget::OverlayMethod::None,
          })
        , mLoading(false)
        , mCurrentCamera(Vector3f::Zero(), Vector3f::UnitZ(), Vector3f::UnitY())
        , mCurrentTravelSpeed(1)
        , mRequestReset(false)
        , mInternalViewWidth(1024)
        , mInternalViewHeight(1024)
        , mColorbar()
        , mShowColorbar(true)
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

    void onContentResize(size_t width, size_t height)
    {
        updateSize(width, height);
        setupTextureBuffer(width, height);
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
            const auto start = std::chrono::high_resolution_clock::now();
            if (mRequestReset) {
                mRuntime->setCameraOrientation(mCurrentCamera.asOrientation());
                mRuntime->reset();
                mRequestReset = false;
            }
            mRuntime->step();

            ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowDockID(sMainWindowDockID, ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Render", nullptr, ImGuiWindowFlags_NoCollapse)) {
                const ImVec2 topLeft = ImGui::GetCursorPos();
                if (mTexture)
                    runPipeline();

                const ImVec2 contentMin  = ImGui::GetWindowContentRegionMin();
                const ImVec2 contentMax  = ImGui::GetWindowContentRegionMax();
                const ImVec2 contentSize = ImVec2(contentMax.x - contentMin.x, contentMax.y - contentMin.y);
                if (!mTexture || mWidth != contentSize.x || mHeight != contentSize.y)
                    onContentResize((size_t)std::max(0.0f, contentSize.x), (size_t)std::max(0.0f, contentSize.y));

                handleInput();

                if (mShowColorbar && mCurrentParameters.allowColorbar()) {
                    ImGui::SetCursorPos(topLeft);

                    const float min = mCurrentParameters.OverlayMethod == RenderWidget::OverlayMethod::GlareSource ? (mRuntime->parameters().getFloat("_luminance_avg") * mRuntime->parameters().getFloat("_glare_multiplier")) : mRuntime->parameters().getFloat("_luminance_softmin");
                    const float max = mCurrentParameters.OverlayMethod == RenderWidget::OverlayMethod::GlareSource ? mRuntime->parameters().getFloat("_luminance_max") : mRuntime->parameters().getFloat("_luminance_softmax");
                    mColorbar.render(min, max);
                }
            }
            ImGui::End();

            const auto end = std::chrono::high_resolution_clock::now();
            mCurrentFPS    = 1000.0f / std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            if (!std::isfinite(mCurrentFPS))
                mCurrentFPS = 0;
        }
    }

    inline void loadFromFile(const Path& path)
    {
        auto options              = RuntimeOptions::makeDefault();
        options.IsInteractive     = true;
        options.EnableTonemapping = false; // We do our own stuff
        options.OverrideFilmSize  = { (uint32)mInternalViewWidth, (uint32)mInternalViewHeight };

        SceneParser parser;
        auto scene = parser.loadFromFile(path);

        auto prevCamera = scene->camera();

        auto cameraObject = std::make_shared<SceneObject>(SceneObject::OT_CAMERA, "fisheye", Path{});
        cameraObject->setProperty("mode", SceneProperty::fromString("circular"));
        if (prevCamera && prevCamera->hasProperty("transform"))
            cameraObject->setProperty("transform", prevCamera->property("transform"));
        if (prevCamera && prevCamera->hasProperty("fov")) // TODO: Other types?
            mCurrentParameters.FOV = prevCamera->property("fov").getNumber(mCurrentParameters.FOV);

        scene->setCamera(cameraObject);

        mRuntime = std::make_unique<Runtime>(options);
        if (mRuntime->loadFromScene(scene.get())) {
            setupPerspectivePass(path, scene.get());
            setupImageInfoPass(path, scene.get());
            setupTonemapPass(path, scene.get());
            setupGlarePass(path, scene.get());
            setupOverlayPass(path, scene.get());
            setupAOVPass(path, scene.get());

            updateSize(mWidth, mHeight);
            updateParameters(mCurrentParameters);

            mCurrentCamera = CameraProxy(mRuntime->initialCameraOrientation());

            auto bbox    = mRuntime->sceneBoundingBox();
            mSceneCenter = bbox.center();

            bbox.extend(mCurrentCamera.Eye);
            mCurrentTravelSpeed = std::max(1e-4f, bbox.diameter().maxCoeff() / 50);
        } else {
            mRuntime.reset();
        }
        mRequestReset = false;

        mLoading = false;
    }

    inline void updateParameters(const RenderWidget::Parameters& params)
    {
        mCurrentParameters = params;
        if (mRuntime) {
            mRuntime->setParameter("_canvas_fov", mCurrentParameters.FOV);
            mRuntime->setParameter("_overlay_method", (int)mCurrentParameters.OverlayMethod);
        }

        if (mTonemapPass) {
            mTonemapPass->setParameter("_tonemap_method", (int)mCurrentParameters.ToneMappingMethod);
            mTonemapPass->setParameter("_tonemap_exposure_factor", std::pow(2.0f, mCurrentParameters.ExposureFactor));
            mTonemapPass->setParameter("_tonemap_exposure_offset", mCurrentParameters.ExposureOffset);
        }
    }

    inline const RenderWidget::Parameters& currentParameters() const { return mCurrentParameters; }
    inline Runtime* currentRuntime() { return mRuntime.get(); }
    inline float currentFPS() const { return mCurrentFPS; }

    inline void cleanup()
    {
        mPerspectivePass.reset();
        mImageInfoPass.reset();
        mTonemapPass.reset();
        mGlarePass.reset();
        mOverlayPass.reset();
        mAOVPass.reset();
        mRuntime.reset();
    }

    void resizeInternalView(size_t width, size_t height)
    {
        if (width == 0 || height == 0)
            return;

        mInternalViewWidth  = width;
        mInternalViewHeight = height;

        if (mRuntime)
            mRuntime->resizeFramebuffer(width, height);
    }

    inline std::pair<size_t, size_t> internalViewSize() const
    {
        return std::make_pair(mInternalViewWidth, mInternalViewHeight);
    }

    inline bool isColorbarVisible() const { return mShowColorbar; }
    inline void showColorbar(bool b) { mShowColorbar = b; }

private:
    void handleInput()
    {
        if (!mRuntime)
            return;

        const ImGuiIO& io = ImGui::GetIO();

        if (!io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad1, false))
                mCurrentCamera.update_dir(Vector3f(0, 0, 1), Vector3f(0, 1, 0));
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad3, false))
                mCurrentCamera.update_dir(Vector3f(1, 0, 0), Vector3f(0, 1, 0));
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad7, false))
                mCurrentCamera.update_dir(Vector3f(0, 1, 0), Vector3f(0, 0, 1));
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad9, false))
                mCurrentCamera.update_dir(-mCurrentCamera.Direction, mCurrentCamera.Up);
            if (ImGui::IsKeyPressed(ImGuiKey_O, true))
                mCurrentCamera.snap_up();
            if (ImGui::IsKeyPressed(ImGuiKey_R, false))
                mCurrentCamera = CameraProxy(mRuntime->initialCameraOrientation());

            if (ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_W))
                mCurrentCamera.move(0, 0, mCurrentTravelSpeed);
            if (ImGui::IsKeyDown(ImGuiKey_DownArrow) || ImGui::IsKeyDown(ImGuiKey_S))
                mCurrentCamera.move(0, 0, -mCurrentTravelSpeed);
            if (ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_A))
                mCurrentCamera.move(-mCurrentTravelSpeed, 0, 0);
            if (ImGui::IsKeyDown(ImGuiKey_RightArrow) || ImGui::IsKeyDown(ImGuiKey_D))
                mCurrentCamera.move(mCurrentTravelSpeed, 0, 0);
            if (ImGui::IsKeyDown(ImGuiKey_E))
                mCurrentCamera.roll(KRSPEED);
            if (ImGui::IsKeyDown(ImGuiKey_Q))
                mCurrentCamera.roll(-KRSPEED);
            if (ImGui::IsKeyDown(ImGuiKey_PageUp))
                mCurrentCamera.move(0, mCurrentTravelSpeed, 0);
            if (ImGui::IsKeyDown(ImGuiKey_PageDown))
                mCurrentCamera.move(0, -mCurrentTravelSpeed, 0);
            if (ImGui::IsKeyDown(ImGuiKey_Keypad2))
                handleRotation(0, KRSPEED);
            if (ImGui::IsKeyDown(ImGuiKey_Keypad8))
                handleRotation(0, -KRSPEED);
            if (ImGui::IsKeyDown(ImGuiKey_Keypad4))
                handleRotation(-KRSPEED, 0);
            if (ImGui::IsKeyDown(ImGuiKey_Keypad6))
                handleRotation(KRSPEED, 0);

            if (ImGui::IsKeyDown(ImGuiKey_KeypadAdd))
                mCurrentTravelSpeed *= 1.1f;
            if (ImGui::IsKeyDown(ImGuiKey_KeypadSubtract))
                mCurrentTravelSpeed *= 0.9f;
        }

        // Note: Not sure why io.WantMouseCapture does not work as intended... or I misunderstood something
        if (io.WantCaptureMouse && ImGui::IsItemHovered(ImGuiHoveredFlags_RootAndChildWindows)) {
            if (io.MouseWheel != 0)
                mCurrentCamera.move(0, 0, io.MouseWheel * mCurrentTravelSpeed);

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
                const ImVec2 lookDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0);
                const float aspeed     = RSPEED;
                handleRotation(lookDelta.x * aspeed, lookDelta.y * aspeed);
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            }

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)) {
                const ImVec2 panDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right, 0);
                const float aspeed    = mCurrentTravelSpeed / 10;
                mCurrentCamera.move(panDelta.x * aspeed, -panDelta.y * aspeed, 0);
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
            }
        }

        const auto previousCamera = mRuntime->getCameraOrientation();

        mRequestReset = previousCamera.Eye != mCurrentCamera.Eye
                        || previousCamera.Up != mCurrentCamera.Up
                        || previousCamera.Dir != mCurrentCamera.Direction;
    }

    inline void handleRotation(float xmotion, float ymotion)
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && io.KeyAlt) {
            mCurrentCamera.rotate_around(mSceneCenter, xmotion, ymotion);
            mCurrentCamera.snap_up();
        } else if (io.KeyAlt) {
            mCurrentCamera.rotate_fixroll(xmotion, ymotion);
        } else if (io.KeyCtrl) {
            mCurrentCamera.rotate_around(mSceneCenter, xmotion, ymotion);
        } else {
            mCurrentCamera.rotate(xmotion, ymotion);
        }
    }

    inline void updateSize(size_t width, size_t height)
    {
        mWidth  = width;
        mHeight = height;
        if (mRuntime) {
            mRuntime->setParameter("_canvas_width", (int)mWidth);
            mRuntime->setParameter("_canvas_height", (int)mHeight);
        }
    }

    inline bool runPipeline()
    {
        if (mWidth == 0 || mHeight == 0)
            return false;

        IG_ASSERT(mBuffer.size() >= mWidth * mHeight, "Invalid buffer");

        if (!mGlarePass->run()) {
            IG_LOG(L_FATAL) << "Failed to run glare pass" << std::endl;
            return false;
        }

        if (!mPerspectivePass->run()) {
            IG_LOG(L_FATAL) << "Failed to run perspective pass" << std::endl;
            return false;
        }

        if (!mImageInfoPass->run()) {
            IG_LOG(L_FATAL) << "Failed to run imageinfo pass" << std::endl;
            return false;
        }

        if (mCurrentParameters.OverlayMethod == RenderWidget::OverlayMethod::Normal) {
            mRuntime->setParameter("_aov", "Normals");
            if (!mAOVPass->run()) {
                IG_LOG(L_FATAL) << "Failed to run aov pass" << std::endl;
                return false;
            }
            mAOVPass->copyOutputToHost("_final_output", mBuffer.data(), mWidth * mHeight * sizeof(uint32));
        } else if (mCurrentParameters.OverlayMethod == RenderWidget::OverlayMethod::Albedo) {
            mRuntime->setParameter("_aov", "Albedo");
            if (!mAOVPass->run()) {
                IG_LOG(L_FATAL) << "Failed to run aov pass" << std::endl;
                return false;
            }
            mAOVPass->copyOutputToHost("_final_output", mBuffer.data(), mWidth * mHeight * sizeof(uint32));
        } else {
            if (mCurrentParameters.OverlayMethod == RenderWidget::OverlayMethod::None
                || mCurrentParameters.OverlayMethod == RenderWidget::OverlayMethod::GlareSource) {
                if (!mTonemapPass->run()) {
                    IG_LOG(L_FATAL) << "Failed to run tonemap pass" << std::endl;
                    return false;
                }
            }

            if (mCurrentParameters.OverlayMethod != RenderWidget::OverlayMethod::None) {
                if (!mOverlayPass->run()) {
                    IG_LOG(L_FATAL) << "Failed to run overlay pass" << std::endl;
                    return false;
                }
                mOverlayPass->copyOutputToHost("_final_output", mBuffer.data(), mWidth * mHeight * sizeof(uint32));
            } else {
                mTonemapPass->copyOutputToHost("_final_output", mBuffer.data(), mWidth * mHeight * sizeof(uint32));
            }
        }

        SDL_UpdateTexture(mTexture, nullptr, mBuffer.data(), static_cast<int>(mWidth * sizeof(uint32)));
        ImGui::Image((void*)mTexture, ImVec2((float)mWidth, (float)mHeight));

        return true;
    }

    inline std::shared_ptr<RenderPass> createPass(const std::string& suffixSrc) const
    {
        std::stringstream shader;
        for (int i = 0; ig_shader[i]; ++i)
            shader << ig_shader[i] << std::endl;

        shader << suffixSrc;
        return mRuntime->createPass(shader.str());
    }

    inline bool setupPerspectivePass(const Path& path, Scene* scene)
    {
        IG_LOG(L_DEBUG) << "Compiling perspective pass" << std::endl;
        auto loaderOptions     = mRuntime->loaderOptions();
        loaderOptions.FilePath = path;
        loaderOptions.Scene    = scene;

        mPerspectivePass = createPass(ShaderGenerator::generatePerspective(loaderOptions));
        if (!mPerspectivePass) {
            IG_LOG(L_ERROR) << "Could not setup perspective pass" << std::endl;
            return false;
        }

        return true;
    }

    inline bool setupImageInfoPass(const Path& path, Scene* scene)
    {
        IG_LOG(L_DEBUG) << "Compiling imageinfo pass" << std::endl;
        auto loaderOptions     = mRuntime->loaderOptions();
        loaderOptions.FilePath = path;
        loaderOptions.Scene    = scene;

        mImageInfoPass = createPass(ShaderGenerator::generateImageInfo(loaderOptions));
        if (!mImageInfoPass) {
            IG_LOG(L_ERROR) << "Could not setup imageinfo pass" << std::endl;
            return false;
        }

        return true;
    }

    inline bool setupTonemapPass(const Path& path, Scene* scene)
    {
        IG_LOG(L_DEBUG) << "Compiling tonemap pass" << std::endl;
        auto loaderOptions     = mRuntime->loaderOptions();
        loaderOptions.FilePath = path;
        loaderOptions.Scene    = scene;

        mTonemapPass = createPass(ShaderGenerator::generateTonemap(loaderOptions));
        if (!mTonemapPass) {
            IG_LOG(L_ERROR) << "Could not setup tonemap pass" << std::endl;
            return false;
        }

        return true;
    }

    inline bool setupGlarePass(const Path& path, Scene* scene)
    {
        IG_LOG(L_DEBUG) << "Compiling glare pass" << std::endl;
        auto loaderOptions     = mRuntime->loaderOptions();
        loaderOptions.FilePath = path;
        loaderOptions.Scene    = scene;

        mGlarePass = createPass(ShaderGenerator::generateGlare(loaderOptions));
        if (!mGlarePass) {
            IG_LOG(L_ERROR) << "Could not setup glare pass" << std::endl;
            return false;
        }

        return true;
    }

    inline bool setupOverlayPass(const Path& path, Scene* scene)
    {
        IG_LOG(L_DEBUG) << "Compiling overlay pass" << std::endl;
        auto loaderOptions     = mRuntime->loaderOptions();
        loaderOptions.FilePath = path;
        loaderOptions.Scene    = scene;

        mOverlayPass = createPass(ShaderGenerator::generateOverlay(loaderOptions));
        if (!mOverlayPass) {
            IG_LOG(L_ERROR) << "Could not setup overlay pass" << std::endl;
            return false;
        }

        mRuntime->setParameter("_glare_multiplier", 1.0f);
        mRuntime->setParameter("_glare_vertical_illuminance", -1.0f /* Automatic */);

        return true;
    }

    inline bool setupAOVPass(const Path& path, Scene* scene)
    {
        IG_LOG(L_DEBUG) << "Compiling aov pass" << std::endl;
        auto loaderOptions     = mRuntime->loaderOptions();
        loaderOptions.FilePath = path;
        loaderOptions.Scene    = scene;

        mAOVPass = createPass(ShaderGenerator::generateAOV(loaderOptions));
        if (!mAOVPass) {
            IG_LOG(L_ERROR) << "Could not setup aov pass" << std::endl;
            return false;
        }

        return true;
    }

    // Buffer stuff
    inline bool setupTextureBuffer(size_t width, size_t height)
    {
        if (width == 0 || height == 0)
            return false;

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
    RenderWidget::Parameters mCurrentParameters;

    std::unique_ptr<Runtime> mRuntime;
    std::shared_ptr<RenderPass> mPerspectivePass;
    std::shared_ptr<RenderPass> mImageInfoPass;
    std::shared_ptr<RenderPass> mTonemapPass;
    std::shared_ptr<RenderPass> mGlarePass;
    std::shared_ptr<RenderPass> mOverlayPass;
    std::shared_ptr<RenderPass> mAOVPass;
    Path mRequestedPath;

    std::unique_ptr<std::thread> mLoadingThread;
    std::atomic<bool> mLoading;

    float mCurrentFPS;
    CameraProxy mCurrentCamera;

    Vector3f mSceneCenter;
    float mCurrentTravelSpeed;
    bool mRequestReset;

    size_t mInternalViewWidth;
    size_t mInternalViewHeight;

    ColorbarGizmo mColorbar;
    bool mShowColorbar;
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

void RenderWidget::cleanup()
{
    mInternal->cleanup();
}

void RenderWidget::openFile(const Path& path)
{
    mInternal->openFile(path);
}

void RenderWidget::onRender(Widget*)
{
    mInternal->onRender();
}

void RenderWidget::updateParameters(const Parameters& params)
{
    mInternal->updateParameters(params);
}

const RenderWidget::Parameters& RenderWidget::currentParameters() const
{
    return mInternal->currentParameters();
}

Runtime* RenderWidget::currentRuntime()
{
    return mInternal->currentRuntime();
}

float RenderWidget::currentFPS() const
{
    return mInternal->currentFPS();
}

void RenderWidget::resizeInternalView(size_t width, size_t height)
{
    mInternal->resizeInternalView(width, height);
}

std::pair<size_t, size_t> RenderWidget::internalViewSize() const
{
    return mInternal->internalViewSize();
}

bool RenderWidget::isColorbarVisible() const { return mInternal->isColorbarVisible(); }
void RenderWidget::showColorbar(bool b) { mInternal->showColorbar(b); }
}; // namespace IG