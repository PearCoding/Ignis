#include "UI.h"

#include "Timeline.h"
#include "UIGlue.h"

#include "Color.h"
#include "Logger.h"
#include "statistics/Statistics.h"

#include <algorithm>

namespace IG {

constexpr float BarHeight = 16;

struct Entry {
    Statistics::TimestampType Type;
    std::vector<Entry> Children = {};
};

static const Entry sEntryRoot = {
    SmallShaderKey(ShaderType::Device),
    { { SectionType::GPUSortPrimary,
        { { SectionType::GPUSortPrimaryReset }, { SectionType::GPUSortPrimaryCount }, { SectionType::GPUSortPrimaryScan }, { SectionType::GPUSortPrimarySort }, { SectionType::GPUSortPrimaryCollapse } } },
      { SectionType::GPUSortSecondary },
      { SectionType::GPUCompactPrimary },
      { SmallShaderKey(ShaderType::PrimaryTraversal) },
      { SmallShaderKey(ShaderType::SecondaryTraversal) },
      { SmallShaderKey(ShaderType::RayGeneration) },
      { SmallShaderKey(ShaderType::Hit, 1 /* Indicates that it can have dynamic ids*/) },
      { SmallShaderKey(ShaderType::Miss) },
      { SmallShaderKey(ShaderType::AdvancedShadowHit, 1) },
      { SmallShaderKey(ShaderType::AdvancedShadowMiss) },
      { SmallShaderKey(ShaderType::Callback, 1) },
      { SmallShaderKey(ShaderType::ImageInfo), { { SectionType::ImageInfoPercentile }, { SectionType::ImageInfoError }, { SectionType::ImageInfoHistogram } } },
      { SmallShaderKey(ShaderType::Tonemap) },
      { SmallShaderKey(ShaderType::Glare) },
      { SmallShaderKey(ShaderType::Bake) },
      { SectionType::Denoise },
      { SectionType::ImageLoading },
      { SectionType::PackedImageLoading },
      { SectionType::BufferLoading },
      { SectionType::BufferRequests },
      { SectionType::BufferReleases },
      { SectionType::FramebufferUpdate },
      { SectionType::AOVUpdate },
      { SectionType::TonemapUpdate },
      { SectionType::FramebufferHostUpdate },
      { SectionType::AOVHostUpdate } }
};

class UIInternal {
public:
    Statistics Stats;
    float TotalMS       = 0;
    float StreamStartMS = std::numeric_limits<float>::infinity();
    float StreamEndMS   = 0;

    UI* Parent             = nullptr;
    SDL_Window* Window     = nullptr;
    SDL_Renderer* Renderer = nullptr;

    bool ShowMeans      = true;
    bool ShowQuantities = true;
    bool ShowChart      = true;

    // Events
    UI::InputResult handleEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            IGGui::processSDLEvent(event);

            // First handle ImGui stuff
            switch (event.type) {
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    return UI::InputResult::Quit;
                case SDLK_F2:
                    ShowChart = !ShowChart;
                    break;
                case SDLK_F3:
                    ShowQuantities = !ShowQuantities;
                    break;
                case SDLK_F4:
                    ShowMeans = !ShowMeans;
                    break;
                default:
                    break;
                }
                break;
            case SDL_QUIT:
                return UI::InputResult::Quit;
            case SDL_WINDOWEVENT: {
                switch (event.window.event) {
                case SDL_WINDOWEVENT_RESIZED:
                    IGGui::notifyResize(Window, Renderer);
                    break;
                default:
                    break;
                }
            }
            default:
                break;
            }
        }

        return UI::InputResult::Continue;
    }

    static constexpr int WindowFlags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;

    void handleQuantityWindow()
    {
        const size_t totalIter = Stats.entry(ShaderType::Device).count;

        if (ImGui::Begin("Quantities")) {
            if (ImGui::BeginTable("table_quantity", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Total");
                ImGui::TableSetupColumn("Per Iteration");
                ImGui::TableSetupColumn("Per Second");
                ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
                ImGui::TableHeadersRow();

                uint64 camera = Stats.quantity(Quantity::CameraRayCount);
                uint64 bounce = Stats.quantity(Quantity::BounceRayCount);
                uint64 shadow = Stats.quantity(Quantity::ShadowRayCount);

                std::array<std::tuple<const char*, uint64>, 6> values = {
                    std::make_tuple("Iteration", (uint64)totalIter),
                    std::make_tuple("Camera Ray Count", camera),
                    std::make_tuple("Bounce Ray Count", bounce),
                    std::make_tuple("Shadow Ray Count", shadow),
                    std::make_tuple("Primary Ray Count", camera + bounce),
                    std::make_tuple("Total Ray Count", camera + bounce + shadow)
                };

                for (size_t i = 0; i < values.size(); ++i) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(std::get<0>(values[i]));

                    uint64 total = std::get<1>(values[i]);
                    ImGui::TableNextColumn();
                    ImGui::Text("%" PRIu64, total);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", static_cast<float>(total) / totalIter);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", static_cast<float>(total) / TotalMS / 1000);
                }

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void handleMeansTreeNode(const Entry& e, bool dynamic, const size_t totalIter)
    {
        bool dynamicChildren = false;
        if (!dynamic && std::holds_alternative<SmallShaderKey>(e.Type))
            dynamicChildren = std::get<SmallShaderKey>(e.Type).subID() > 0;

        const bool isLeaf   = e.Children.empty() && !dynamicChildren;
        const int treeFlags = isLeaf ? (ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth) : (ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);

        Statistics::MeanEntry meanEntry;
        if (std::holds_alternative<SmallShaderKey>(e.Type)) {
            const auto stype = std::get<SmallShaderKey>(e.Type);
            if (!dynamicChildren) {
                meanEntry = Stats.entry(stype.type(), stype.subID());
            } else {
                // Accumulate
                for (auto it = Stats.shadersBegin(); it != Stats.shadersEnd(); ++it) {
                    if (it->type() != stype.type())
                        continue;

                    const auto& shader = Stats.entry(it->type(), it->subID());
                    meanEntry.count += shader.count;
                    meanEntry.totalElapsedMS += shader.totalElapsedMS;
                    meanEntry.totalWorkload += shader.totalWorkload;
                    meanEntry.minWorkload = std::min(meanEntry.minWorkload, shader.minWorkload);
                    meanEntry.maxWorkload = std::max(meanEntry.maxWorkload, shader.maxWorkload);
                }
            }
        } else {
            const auto stype = std::get<SectionType>(e.Type);
            meanEntry        = Stats.entry(stype);
        }

        // Only show nodes with data
        if (meanEntry.count == 0)
            return;

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        bool open;
        if (std::holds_alternative<SmallShaderKey>(e.Type)) {
            const auto stype = std::get<SmallShaderKey>(e.Type);

            if (dynamic) {
                std::string name = "[" + std::to_string(stype.subID()) + "]";
                open             = ImGui::TreeNodeEx(name.c_str(), treeFlags);
            } else {
                open = ImGui::TreeNodeEx(Statistics::getShaderTypeName(stype.type()), treeFlags);
            }
            ImGui::TableNextColumn();
        } else {
            const auto stype = std::get<SectionType>(e.Type);
            open             = ImGui::TreeNodeEx(Statistics::getSectionTypeName(stype), treeFlags);
            ImGui::TableNextColumn();
        }

        ImGui::Text("%.2f ms", meanEntry.totalElapsedMS);
        ImGui::TableNextColumn();
        ImGui::Text("%" PRIu64, meanEntry.count);
        ImGui::TableNextColumn();
        ImGui::Text("%.2f ms", meanEntry.totalElapsedMS / totalIter);
        ImGui::TableNextColumn();
        ImGui::Text("%.2f", static_cast<float>(meanEntry.count) / totalIter);
        ImGui::TableNextColumn();
        ImGui::Text("%.2f ms", meanEntry.totalElapsedMS / meanEntry.count);
        ImGui::TableNextColumn();

        // Handle workloads
        if (std::holds_alternative<SmallShaderKey>(e.Type)) {
            ImGui::Text("%" PRIu64, meanEntry.minWorkload);
            ImGui::TableNextColumn();
            ImGui::Text("%" PRIu64, meanEntry.totalWorkload / meanEntry.count);
            ImGui::TableNextColumn();
            ImGui::Text("%" PRIu64, meanEntry.maxWorkload);
        } else {
            // Skip workloads
            ImGui::TextDisabled("--");
            ImGui::TableNextColumn();
            ImGui::TextDisabled("--");
            ImGui::TableNextColumn();
            ImGui::TextDisabled("--");
        }

        if (open && !isLeaf) {
            if (dynamicChildren) {
                const auto stype = std::get<SmallShaderKey>(e.Type);
                for (auto it = Stats.shadersBegin(); it != Stats.shadersEnd(); ++it) {
                    if (it->type() != stype.type())
                        continue;

                    handleMeansTreeNode(Entry{ *it }, true, totalIter);
                }

            } else {
                for (const auto& c : e.Children)
                    handleMeansTreeNode(c, false, totalIter);
            }
            ImGui::TreePop();
        }
    }

    void handleMeansWindow()
    {
        const size_t totalIter = Stats.entry(ShaderType::Device).count;

        if (ImGui::Begin("Means")) {
            if (ImGui::BeginTable("table_means", 9, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("Total Time");
                ImGui::TableSetupColumn("Total Count");
                ImGui::TableSetupColumn("Avg. Time per Iteration");
                ImGui::TableSetupColumn("Avg. Count per Iteration");
                ImGui::TableSetupColumn("Avg. Time per Call");
                ImGui::TableSetupColumn("Min Workload per Call");
                ImGui::TableSetupColumn("Avg. Workload per Call");
                ImGui::TableSetupColumn("Max Workload per Call");
                ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
                ImGui::TableHeadersRow();

                handleMeansTreeNode(sEntryRoot, false, totalIter);

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void handleChartsTreeNode(const Entry& e, bool dynamic, const size_t totalIter)
    {
        bool dynamicChildren = false;
        if (!dynamic && std::holds_alternative<SmallShaderKey>(e.Type))
            dynamicChildren = std::get<SmallShaderKey>(e.Type).subID() > 0;

        const bool isLeaf   = e.Children.empty() && !dynamicChildren;
        const int treeFlags = isLeaf ? (ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen) : (ImGuiTreeNodeFlags_DefaultOpen);

        // Check if there is even data available
        if (!dynamicChildren) {
            bool hasData = false;
            for (const auto& timestamp : Stats.stream()) {
                if (timestamp.type == e.Type) {
                    hasData = true;
                    break;
                }
            }
            if (!hasData)
                return;
        } else {
            const auto stype = std::get<SmallShaderKey>(e.Type);
            bool hasData     = false;
            for (const auto& timestamp : Stats.stream()) {
                if (std::holds_alternative<SmallShaderKey>(timestamp.type) && std::get<SmallShaderKey>(timestamp.type).type() == stype.type()) {
                    hasData = true;
                    break;
                }
            }
            if (!hasData)
                return;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        IGGui::BeginTimelineRow();

        bool open;
        if (std::holds_alternative<SmallShaderKey>(e.Type)) {
            const auto stype = std::get<SmallShaderKey>(e.Type);

            if (dynamic) {
                std::string name = "[" + std::to_string(stype.subID()) + "]";
                open             = ImGui::TreeNodeEx(name.c_str(), treeFlags);
            } else {
                open = ImGui::TreeNodeEx(Statistics::getShaderTypeName(stype.type()), treeFlags);
            }
            ImGui::TableNextColumn();
        } else {
            const auto stype = std::get<SectionType>(e.Type);
            open             = ImGui::TreeNodeEx(Statistics::getSectionTypeName(stype), treeFlags);
            ImGui::TableNextColumn();
        }

        if (!dynamicChildren) {
            ImGui::PushItemWidth(-1);
            for (const auto& timestamp : Stats.stream()) {
                if (timestamp.type == e.Type) {
                    if (std::holds_alternative<SmallShaderKey>(e.Type))
                        IGGui::TimelineEvent(timestamp.offsetStartMS, timestamp.offsetEndMS, "%.3f ms | %" PRIu64, timestamp.offsetEndMS - timestamp.offsetStartMS, timestamp.workload);
                    else
                        IGGui::TimelineEvent(timestamp.offsetStartMS, timestamp.offsetEndMS, "%.3f ms", timestamp.offsetEndMS - timestamp.offsetStartMS);
                }
            }
            ImGui::PopItemWidth();
        }

        if (open && !isLeaf) {
            if (dynamicChildren) {
                const auto stype = std::get<SmallShaderKey>(e.Type);
                for (auto it = Stats.shadersBegin(); it != Stats.shadersEnd(); ++it) {
                    if (it->type() != stype.type())
                        continue;

                    handleChartsTreeNode(Entry{ *it }, true, totalIter);
                }

            } else {
                for (const auto& c : e.Children)
                    handleChartsTreeNode(c, false, totalIter);
            }
            ImGui::TreePop();
        }

        IGGui::EndTimelineRow();
    }

    void handleChartWindow()
    {
        const size_t totalIter = Stats.entry(ShaderType::Device).count;

        if (ImGui::Begin("Timeline")) {
            if (ImGui::BeginTable("table_timeline", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)) {
                if (IGGui::BeginTimeline("#timeline")) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                    ImGui::TableSetupColumn("TIMELINE SLIDER", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible

                    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                    for (int column = 0; column < 2; column++) {
                        ImGui::TableSetColumnIndex(column);
                        ImGui::PushID(column);
                        if (column == 0) {
                            const char* column_name = ImGui::TableGetColumnName(column); // Retrieve name passed to TableSetupColumn()
                            ImGui::TableHeader(column_name);
                        } else {
                            IGGui::TimelineHeader(StreamStartMS, StreamEndMS, "%.3f ms", true);
                        }
                        ImGui::PopID();
                    }

                    handleChartsTreeNode(sEntryRoot, false, totalIter);

                    for (const auto& timestamp : Stats.stream()) {
                        if (std::holds_alternative<Statistics::Barrier>(timestamp.type)) {
                            const auto barrier = std::get<Statistics::Barrier>(timestamp.type);
                            if (barrier == Statistics::Barrier::Frame)
                                IGGui::TimelineBarrier(timestamp.offsetStartMS, ImColor(128, 128, 255));
                            else
                                IGGui::TimelineBarrier(timestamp.offsetStartMS, ImColor(128, 255, 128));
                        }
                    }
                    IGGui::EndTimeline();
                }

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void handleImgui()
    {
        // ImGui::ShowDemoWindow();

        if (ShowQuantities)
            handleQuantityWindow();

        if (ShowMeans)
            handleMeansWindow();

        if (ShowChart)
            handleChartWindow();
    }
};

////////////////////////////////////////////////////////////////
UI::UI(const Statistics& stats, float total_ms)
    : mInternal(std::make_unique<UIInternal>())
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        IG_LOG(L_FATAL) << "Cannot initialize SDL: " << SDL_GetError() << std::endl;
        throw std::runtime_error("Could not setup UI");
    }

    mInternal->Stats   = stats;
    mInternal->TotalMS = total_ms;
    mInternal->Parent  = this;

    mInternal->StreamStartMS = std::numeric_limits<float>::infinity();
    mInternal->StreamEndMS   = 0;
    for (const auto& ts : stats.stream()) {
        mInternal->StreamStartMS = std::min(mInternal->StreamStartMS, ts.offsetStartMS);
        mInternal->StreamEndMS   = std::max(mInternal->StreamEndMS, ts.offsetEndMS);
    }

    mInternal->Window = SDL_CreateWindow(
        "Ignis Profiler",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        800,
        600,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED);

    if (!mInternal->Window) {
        IG_LOG(L_FATAL) << "Cannot create SDL window: " << SDL_GetError() << std::endl;
        throw std::runtime_error("Could not setup UI");
    }
    SDL_SetWindowMinimumSize(mInternal->Window, 64, 64);

    mInternal->Renderer = SDL_CreateRenderer(mInternal->Window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (!mInternal->Renderer) {
        IG_LOG(L_FATAL) << "Cannot create SDL renderer: " << SDL_GetError() << std::endl;
        throw std::runtime_error("Could not setup UI");
    }

#ifdef IG_OS_LINUX
    // No idea why SDL_WINDOW_MAXIMIZED & SDL_MaximizeWindow is not working as intendent..
    SDL_Rect rect;
    SDL_GetDisplayUsableBounds(SDL_GetWindowDisplayIndex(mInternal->Window), &rect);
    SDL_SetWindowSize(mInternal->Window, rect.w, rect.h);
#endif

    IGGui::setup(mInternal->Window, mInternal->Renderer, true);
}

UI::~UI()
{
    IGGui::shutdown();

    if (mInternal->Renderer)
        SDL_DestroyRenderer(mInternal->Renderer);
    if (mInternal->Window)
        SDL_DestroyWindow(mInternal->Window);
    SDL_Quit();
}

UI::InputResult UI::handleInput()
{
    return mInternal->handleEvents();
}

void UI::update()
{
    SDL_RenderClear(mInternal->Renderer);

    IGGui::newFrame();

    mInternal->handleImgui();

    IGGui::renderFrame();
    SDL_RenderPresent(mInternal->Renderer);
}
} // namespace IG