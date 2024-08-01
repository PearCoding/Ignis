#pragma once

#include "IDataModel.h"
#include "Widget.h"

namespace IG {
class MenuItem;
class PropertyWidget : public Widget {
public:
    PropertyWidget();
    virtual ~PropertyWidget();

    void onRender(Widget* parent) override;

    void setModel(const std::shared_ptr<IDataModel> model);
    inline void connectMenuItem(MenuItem* item) { mVisibleItem = item; }

    inline Vector3f currentView() const { return mView; }
    inline DataComponent pickedComponent() const { return mComponent; }

private:
    std::shared_ptr<IDataModel> mModel;
    MenuItem* mVisibleItem;

    Vector3f mView;
    DataComponent mComponent;
};
} // namespace IG