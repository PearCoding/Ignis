#pragma once

#include "Widget.h"

namespace IG {
class IDataModel;
class MenuItem;
class PropertyWidget;
class ViewWidget : public Widget {
public:
    ViewWidget();
    virtual ~ViewWidget();

    void onRender(Widget* parent) override;

    void setModel(const std::shared_ptr<IDataModel> model);

    inline void connectMenuItem(MenuItem* item) { mVisibleItem = item; }
    inline void connectProperties(PropertyWidget* widget) { mPropertyWidget = widget; }

private:
    std::shared_ptr<IDataModel> mModel;
    MenuItem* mVisibleItem;
    PropertyWidget* mPropertyWidget;
};
} // namespace IG