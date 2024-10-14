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

    [[nodiscard]] inline float incidentTheta() const { return mViewTheta; }
    [[nodiscard]] inline float incidentPhi() const { return mViewPhi; }

private:
    std::shared_ptr<IDataModel> mModel;
    MenuItem* mVisibleItem;
    PropertyWidget* mPropertyWidget;

    float mViewTheta;
    float mViewPhi;
};
} // namespace IG