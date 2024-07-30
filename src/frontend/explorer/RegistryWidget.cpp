#include "RegistryWidget.h"
#include "MenuItem.h"
#include "PropertyView.h"
#include "RenderWidget.h"
#include "Runtime.h"

#include "UI.h"

namespace IG {

RegistryWidget::RegistryWidget(RenderWidget* renderWidget)
    : Widget()
    , mVisibleItem(nullptr)
    , mRenderWidget(renderWidget)
{
    IG_ASSERT(renderWidget, "Expected a valid render widget");
}

void RegistryWidget::onRender(Widget*)
{
    Runtime* runtime = mRenderWidget->currentRuntime();
    if (!runtime)
        return;

    if (mVisibleItem && !mVisibleItem->isSelected())
        return;

    bool visibility = mVisibleItem ? mVisibleItem->isSelected() : true;
    if (ImGui::Begin("Registry", mVisibleItem ? &visibility : nullptr)) {
        const bool changed = ui::ui_property_view(runtime, true);
        if (changed)
            runtime->reset();
    }
    ImGui::End();

    if (mVisibleItem)
        mVisibleItem->setSelected(visibility);
}

}; // namespace IG