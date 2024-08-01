#include "PropertyWidget.h"
#include "IDataModel.h"
#include "MenuItem.h"

#include "UI.h"

namespace IG {

PropertyWidget::PropertyWidget()
    : Widget()
    , mModel()
    , mVisibleItem(nullptr)
    , mView(Vector3f::UnitZ())
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

        if (mModel) {
            ImGui::Separator();
            mModel->renderProperties(mComponent);
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