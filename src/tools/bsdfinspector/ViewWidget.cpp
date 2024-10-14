#include "ViewWidget.h"
#include "IDataModel.h"
#include "MenuItem.h"
#include "PropertyWidget.h"

#include "UI.h"

namespace IG {
ViewWidget::ViewWidget()
    : Widget()
    , mModel()
    , mVisibleItem(nullptr)
    , mPropertyWidget(nullptr)
    , mViewTheta(0)
    , mViewPhi(0)
{
}

ViewWidget::~ViewWidget()
{
}

void ViewWidget::onRender(Widget*)
{
    IG_ASSERT(mPropertyWidget, "Expected property widget to be valid");

    if (mVisibleItem && !mVisibleItem->isSelected())
        return;

    if (!mModel)
        return;

    bool visibility = mVisibleItem ? mVisibleItem->isSelected() : true;
    if (ImGui::Begin("View", mVisibleItem ? &visibility : nullptr)) {
        auto& drawList = *ImGui::GetWindowDrawList();
        drawList.AddRectFilled(ImGui::GetCursorScreenPos(), ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x, ImGui::GetCursorScreenPos().y + ImGui::GetContentRegionAvail().y), ImColor(240, 240, 240));

        bool hasView = false;
        if (ImGui::BeginChild("##view_child")) {
            const ImVec2 area  = ImGui::GetContentRegionAvail();
            const float radius = std::min(area.x, area.y) / 2 - 10;
            if (radius > 1) {
                mModel->renderView(mViewTheta, mViewPhi, radius, mPropertyWidget->pickedComponent(), mPropertyWidget->colormapper());
                hasView = true;
            }
        }
        ImGui::EndChild();

        if (hasView) {
            const ImVec2 area   = ImGui::GetItemRectSize();
            const ImVec2 center = ImVec2((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) / 2, (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) / 2);
            const float radius  = std::min(area.x, area.y) / 2 - 10;
            const ImVec2 rpos   = ImVec2((ImGui::GetMousePos().x - center.x) / radius, (ImGui::GetMousePos().y - center.y) / radius);

            if (rpos.x * rpos.x + rpos.y * rpos.y < 1) {
                const float r     = std::sqrt(rpos.x * rpos.x + rpos.y * rpos.y);
                const float theta = std::sqrt(rpos.x * rpos.x + rpos.y * rpos.y) * Pi2;
                float phi         = std::atan2(rpos.y / r, rpos.x / r);
                if (phi < 0)
                    phi += 2 * Pi;

                if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    mViewTheta = theta;
                    mViewPhi   = phi;
                } else {
                    mModel->renderInfo(mViewTheta, mViewPhi, theta, phi, mPropertyWidget->pickedComponent());
                }
            }
        }
    }
    ImGui::End();

    if (mVisibleItem)
        mVisibleItem->setSelected(visibility);
}

void ViewWidget::setModel(const std::shared_ptr<IDataModel> model)
{
    mModel = model;
}
} // namespace IG