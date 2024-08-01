#pragma once

#include "IDataModel.h"
#include "Widget.h"

namespace IG {
class MenuItem;
class ViewWidget;
class PropertyWidget : public Widget {
public:
    PropertyWidget();
    virtual ~PropertyWidget();

    void onRender(Widget* parent) override;

    void setModel(const std::shared_ptr<IDataModel> model);
    inline void connectMenuItem(MenuItem* item) { mVisibleItem = item; }
    inline void connectView(ViewWidget* widget) { mViewWidget = widget; }

    inline DataComponent pickedComponent() const { return mComponent; }

private:
    std::shared_ptr<IDataModel> mModel;
    MenuItem* mVisibleItem;
    ViewWidget* mViewWidget;

    DataComponent mComponent;
};
} // namespace IG