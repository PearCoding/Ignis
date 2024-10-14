#include "PropertyWidget.h"
#include "IDataModel.h"
#include "MenuItem.h"
#include "RangeSlider.h"
#include "ViewWidget.h"

#include "UI.h"

namespace IG {

PropertyWidget::PropertyWidget()
    : Widget()
    , mModel()
    , mVisibleItem(nullptr)
    , mViewWidget(nullptr)
    , mComponent(DataComponent::FrontReflection)
{
}

PropertyWidget::~PropertyWidget()
{
}

static const char* const ComponentOptions[] = {
    "Front Reflection", "Front Transmission", "Back Reflection", "Back Transmission"
};

void PropertyWidget::onRender(Widget*)
{
    IG_ASSERT(mViewWidget, "Expected property widget to be valid");

    if (mVisibleItem && !mVisibleItem->isSelected())
        return;

    bool visibility = mVisibleItem ? mVisibleItem->isSelected() : true;
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Properties", mVisibleItem ? &visibility : nullptr)) {
        const char* current_comp = ComponentOptions[(int)mComponent];
        if (ImGui::BeginCombo("Component", current_comp)) {
            for (int i = 0; i < IM_ARRAYSIZE(ComponentOptions); ++i) {
                bool is_selected = (current_comp == ComponentOptions[i]);
                if (ImGui::Selectable(ComponentOptions[i], is_selected))
                    mComponent = (DataComponent)i;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (ImGui::CollapsingHeader("Colormap", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Logarithmic", &mColormapper.isLogarithmic());
            ui::RangeSliderFloat("Range##colormap_range", &mColormapper.min(), &mColormapper.max(), 0, 1000);
        }

        if (mModel) {
            ImGui::Separator();
            mModel->renderProperties(mViewWidget->incidentTheta(), mViewWidget->incidentPhi(), mComponent);
        }
    }
    ImGui::End();

    if (mVisibleItem)
        mVisibleItem->setSelected(visibility);
}

void PropertyWidget::setModel(const std::shared_ptr<IDataModel> model)
{
    mModel = model;
}
}; // namespace IG