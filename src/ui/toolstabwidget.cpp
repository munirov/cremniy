#include <QFile>
#include <QTabBar>
#include <utility>
#include <qboxlayout.h>
#include <qfileinfo.h>

#include "core/ToolTab.h"
#include "core/FileDataBuffer.h"
#include "core/ToolTabFactory.h"
#include "toolstabwidget.h"

ToolsTabWidget::ToolsTabWidget(QWidget *parent, QString path)
    : QTabWidget(parent)
    , m_filePath(std::move(path))
{
    setTabsClosable(true);


    // Создаем общий буфер данных для всех вкладок
    m_sharedBuffer = new FileDataBuffer(this);

    m_sharedBuffer->openFile(m_filePath);

    auto& toolFactory = ToolTabFactory::instance();

    for (const auto& descriptor : toolFactory.availableTabs()) {
        if (descriptor.autoOpen) {
            createToolTab(descriptor.id);
        }
    }

    if (this->count() > 0) {
        ToolTab* tab = dynamic_cast<ToolTab*>(this->widget(0));
        if (tab) {
            qDebug() << "ToolsTabWidget ctor: preloading first tab" << tab << tab->toolName();
            tab->setTabData();
            tab->setProperty("tabDataLoaded", true);
        }
    }

    connect(this, &QTabWidget::currentChanged, this, [this](int index) {
        if (index < 0)
            return;

        ToolTab* tab = dynamic_cast<ToolTab*>(this->widget(index));
        if (!tab)
            return;

        if (!tab->property("tabDataLoaded").toBool()) {
            qDebug() << "ToolsTabWidget currentChanged: loading tab index=" << index << " ptr=" << tab << " name=" << tab->toolName();
            tab->setTabData();
            tab->setProperty("tabDataLoaded", true);
        }
    });

    connect(this, &QTabWidget::tabCloseRequested, this, &ToolsTabWidget::closeToolTab);

    // // - - Connects - -

    // // Trigger: Menu Bar: File->SaveFile or CTRL+S - saveTabData
    // connect(GlobalWidgetsManager::instance().get_IDEWindow_menuBar_file_saveFile(),
            // &QAction::triggered, this, &ToolsTabWidget::saveCurrentTabData);
}

void ToolsTabWidget::updateCloseButtons()
{
    for (int index = 0; index < count(); ++index) {
        if (widget(index)->property("toolTabClosable").toBool()) {
            continue;
        }

        tabBar()->setTabButton(index, QTabBar::LeftSide, nullptr);
        tabBar()->setTabButton(index, QTabBar::RightSide, nullptr);
    }
}

ToolTab* ToolsTabWidget::findToolTab(const QString& toolId) const
{
    for (int index = 0; index < count(); ++index) {
        auto* tab = qobject_cast<ToolTab*>(widget(index));
        if (tab && tab->property("toolTabId").toString() == toolId) {
            return tab;
        }
    }

    return nullptr;
}

ToolTab* ToolsTabWidget::createToolTab(const QString& toolId)
{
    const auto descriptor = ToolTabFactory::instance().descriptor(toolId);
    if (!descriptor.isValid()) {
        qWarning() << "ToolsTabWidget: unknown tool tab id" << toolId;
        return nullptr;
    }

    ToolTab* tab = ToolTabFactory::instance().create(toolId, m_sharedBuffer);
    if (!tab) {
        qWarning() << "ToolsTabWidget: failed to create tab for id" << toolId;
        return nullptr;
    }

    tab->setFile(m_filePath);
    tab->setProperty("toolTabId", descriptor.id);
    tab->setProperty("toolTabOrder", descriptor.order);
    tab->setProperty("toolTabClosable", descriptor.group == ToolTabGroup::Other);
    tab->setProperty("tabDataLoaded", false);

    connect(tab, &ToolTab::refreshDataAllTabsSignal, this, &ToolsTabWidget::refreshDataAllTabs);
    connect(tab, &ToolTab::modifyData, this, &ToolsTabWidget::setupStar);
    connect(tab, &ToolTab::dataEqual, this, &ToolsTabWidget::removeStar);

    int insertIndex = count();
    if (descriptor.group == ToolTabGroup::Always) {
        for (int index = 0; index < count(); ++index) {
            QWidget* existingWidget = widget(index);
            const bool existingClosable = existingWidget->property("toolTabClosable").toBool();
            const int existingOrder = existingWidget->property("toolTabOrder").toInt();
            if (existingClosable || descriptor.order < existingOrder) {
                insertIndex = index;
                break;
            }
        }
    }

    insertTab(insertIndex, tab, tab->toolIcon(), tab->toolName());
    updateCloseButtons();
    return tab;
}

ToolTab* ToolsTabWidget::openToolTab(const QString& toolId, bool activate)
{
    ToolTab* tab = findToolTab(toolId);
    if (!tab) {
        tab = createToolTab(toolId);
    }

    if (!tab) {
        return nullptr;
    }

    if (!tab->property("tabDataLoaded").toBool()) {
        tab->setTabData();
        tab->setProperty("tabDataLoaded", true);
    }

    if (activate) {
        setCurrentWidget(tab);
    }

    return tab;
}

void ToolsTabWidget::refreshDataAllTabs(){
    for (int tabIndex = 0; tabIndex < this->count(); tabIndex++){
        if (tabIndex != this->currentIndex()){
            ToolTab* tab = dynamic_cast<ToolTab*>(this->widget(tabIndex));
            tab->setTabData();
        }
    }
}

void ToolsTabWidget::closeToolTab(int index)
{
    QWidget* toolWidget = widget(index);
    if (!toolWidget || !toolWidget->property("toolTabClosable").toBool()) {
        return;
    }

    removeTab(index);
    toolWidget->deleteLater();
    updateCloseButtons();
}

void ToolsTabWidget::saveCurrentTabData(){
    ToolTab* tab = dynamic_cast<ToolTab*>(currentWidget());
    if (tab) tab->saveTabData();
}

void ToolsTabWidget::removeStar(){

    qDebug() << "ToolsTabWidget: removeStar() this=" << this << " sender=" << sender() << " currentIndex=" << currentIndex() << " count=" << count();

    // remove star at sender
    QObject* obj = sender();
    QWidget* widget = qobject_cast<QWidget*>(obj);

    if (!widget) {
        qWarning() << "ToolsTabWidget: removeStar(): sender is not QWidget";
        return;
    }

    int index = indexOf(widget);
    if (index < 0) {
        qWarning() << "ToolsTabWidget: removeStar(): widget not found in tab widget" << widget;
        return;
    }

    QString text = tabText(index);
    if (text.endsWith('*')) text.chop(1);
    setTabText(index, text);

    int toolCount_WithoutModIndicator = 0;
    for (int tabIndex = 0; tabIndex < this->count(); tabIndex++){
        if (tabIndex != this->currentIndex()){
            ToolTab* tab = dynamic_cast<ToolTab*>(this->widget(tabIndex));
            if (!tab) {
                qWarning() << "ToolsTabWidget: removeStar(): null tab at index" << tabIndex;
                continue;
            }
            qDebug() << "ToolsTabWidget: removeStar(): " << tab->toolName();
            if (!tab->getModifyIndicator()) {
                qDebug() << "ToolsTabWidget: removeStar(): toolCount_WithoutModIndicator++";
                toolCount_WithoutModIndicator++;
            }
        }
    }

    qDebug() << "ToolsTabWidget: removeStar(): " << toolCount_WithoutModIndicator << " : " << this->count();

    if (toolCount_WithoutModIndicator == (this->count()-1)) {
        qDebug() << "ToolsTabWidget: removeStar(): removeStarSignal";
        emit removeStarSignal();
        qDebug() << "ToolsTabWidget: removeStar(): removeStarSignal returned";
    }

}

void ToolsTabWidget::setupStar(){

    qDebug() << "ToolsTabWidget: setupStar()";

    // setup star on tab
    QObject* obj = sender();
    QWidget* widget = qobject_cast<QWidget*>(obj);

    if (!widget) return;

    int index = indexOf(widget);
    if (index < 0) return;

    QString text = tabText(index);
    if (!text.endsWith("*")){
        setTabText(index, text + "*");
    }

    // signal "setup star" to up
    emit setupStarSignal();

}
