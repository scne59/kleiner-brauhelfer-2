#include "tababstract.h"

extern Settings *gSettings;

TabAbstract::TabAbstract(QWidget *parent) :
    QWidget(parent),
    mIsTabActive(false)
{
}

void TabAbstract::saveSettings()
{
}

void TabAbstract::restoreView(bool full)
{
    Q_UNUSED(full)
}

void TabAbstract::modulesChanged()
{
}

void TabAbstract::setVisibleModule(Settings::Module module, const QVector<QWidget*>& widgets)
{
    bool visible = gSettings->module(module);
    for (const auto& it : widgets)
        it->setVisible(visible);
}

void TabAbstract::setTabActive(bool active)
{
    if (mIsTabActive != active)
    {
        mIsTabActive = active;
        if (mIsTabActive)
            onTabActivated();
        else
            onTabDeactivated();
    }
}

bool TabAbstract::isTabActive() const
{
    return mIsTabActive;
}

void TabAbstract::onTabActivated()
{
}

void TabAbstract::onTabDeactivated()
{
    setFocus();
}

bool TabAbstract::isPrintable() const
{
    return false;
}

void TabAbstract::printPreview()
{
}

void TabAbstract::toPdf()
{
}
