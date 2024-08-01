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

    bool visibility = mVisibleItem ? mVisibleItem->isSelected() : true;
    if (ImGui::Begin("View", mVisibleItem ? &visibility : nullptr)) {
        if (mModel)
            mModel->renderView(mPropertyWidget->currentView(), mPropertyWidget->pickedComponent());
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