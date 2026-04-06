#include <QFile>
#include <qboxlayout.h>
#include <qfileinfo.h>

#include "toolstabwidget.h"
#include "core/ToolTabFactory.h"
#include "core/ToolTab.h"
#include "core/FileDataBuffer.h"

ToolsTabWidget::ToolsTabWidget(QWidget *parent, QString path)
    {

    // Создаем общий буфер данных для всех вкладок
    m_sharedBuffer = new FileDataBuffer(this);

    m_sharedBuffer->openFile(path);

    // Tools

    auto& toolFactory = ToolTabFactory::instance();

    qDebug() << "ToolsTabWidget constr: for id in avTabs";
    for (const QString& toolID : toolFactory.availableTabs()){
        ToolTab* tab = toolFactory.create(toolID, m_sharedBuffer);
        qDebug() << "availableTab: " << tab->toolName();

        tab->setFile(path);
        tab->setProperty("tabDataLoaded", false);

        connect(tab, &ToolTab::refreshDataAllTabsSignal, this, &ToolsTabWidget::refreshDataAllTabs);
        connect(tab, &ToolTab::modifyData, this, &ToolsTabWidget::setupStar);
        connect(tab, &ToolTab::dataEqual, this, &ToolsTabWidget::removeStar);

        if (tab) this->addTab(tab, tab->toolIcon(), tab->toolName());
    }

    if (this->count() > 0) {
        ToolTab* tab = dynamic_cast<ToolTab*>(this->widget(0));
        if (tab) {
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
            tab->setTabData();
            tab->setProperty("tabDataLoaded", true);
        }
    });

    // // - - Connects - -

    // // Trigger: Menu Bar: File->SaveFile or CTRL+S - saveTabData
    // connect(GlobalWidgetsManager::instance().get_IDEWindow_menuBar_file_saveFile(),
            // &QAction::triggered, this, &ToolsTabWidget::saveCurrentTabData);
}

void ToolsTabWidget::refreshDataAllTabs(){
    for (int tabIndex = 0; tabIndex < this->count(); tabIndex++){
        if (tabIndex != this->currentIndex()){
            ToolTab* tab = dynamic_cast<ToolTab*>(this->widget(tabIndex));
            tab->setTabData();
        }
    }
}

void ToolsTabWidget::saveCurrentTabData(){
    ToolTab* tab = dynamic_cast<ToolTab*>(currentWidget());
    if (tab) tab->saveTabData();
}

void ToolsTabWidget::removeStar(){

    qDebug() << "ToolsTabWidget: removeStar()";

    // remove star at sender
    QObject* obj = sender();
    QWidget* widget = qobject_cast<QWidget*>(obj);

    if (!widget) return;

    int index = indexOf(widget);
    if (index < 0) return;

    QString text = tabText(index);
    if (text.endsWith('*')) text.chop(1);
    setTabText(index, text);

    int toolCount_WithoutModIndicator = 0;
    for (int tabIndex = 0; tabIndex < this->count(); tabIndex++){
        if (tabIndex != this->currentIndex()){
            ToolTab* tab = dynamic_cast<ToolTab*>(this->widget(tabIndex));
            qDebug() << "ToolsTabWidget: removeStar(): " << tab->toolName();
            if (!tab->getModifyIndicator()) {
                qDebug() << "ToolsTabWidget: removeStar(): toolCount_WithoutModIndicator++";
                toolCount_WithoutModIndicator++;
            }
        }
    }

    qDebug() << "ToolsTabWidget: removeStar(): " << toolCount_WithoutModIndicator << " : " << this->count();

    if (toolCount_WithoutModIndicator == (this->count()-1)) {
        emit removeStarSignal();
        qDebug() << "ToolsTabWidget: removeStar(): removeStarSignal";
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
