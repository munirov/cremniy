#include <QFile>
#include <qboxlayout.h>
#include <qfileinfo.h>

#include "toolstabwidget.h"
#include "ToolTabs/CodeEditor/codeeditortab.h"
#include "core/ToolTabFactory.h"
#include "core/ToolTab.h"
#include "core/FileDataBuffer.h"

void ToolsTabWidget::activateCodeEditorAtLine(int lineNumber, bool selectWholeLine)
{
    if (lineNumber < 1)
        return;

    for (int i = 0; i < count(); ++i) {
        auto *code = qobject_cast<CodeEditorTab *>(widget(i));
        if (code) {
            setCurrentIndex(i);
            code->goToLine(lineNumber, selectWholeLine);
            return;
        }
    }
}

void ToolsTabWidget::activateCodeEditorSearchHit(int lineNumber, const QString &needle)
{
    if (lineNumber < 1 || needle.isEmpty())
        return;

    for (int i = 0; i < count(); ++i) {
        auto *code = qobject_cast<CodeEditorTab *>(widget(i));
        if (code) {
            setCurrentIndex(i);
            code->goToSearchHit(lineNumber, needle);
            return;
        }
    }
}

ToolsTabWidget::ToolsTabWidget(QWidget *parent, QString path)
    {

    qDebug() << "ToolsTabWidget ctor: this=" << this << " parent=" << parent << " path=" << path;

    // Создаем общий буфер данных для всех вкладок
    m_sharedBuffer = new FileDataBuffer(this);

    m_sharedBuffer->openFile(path);

    // Tools

    auto& toolFactory = ToolTabFactory::instance();

    qDebug() << "ToolsTabWidget constr: for id in avTabs";
    for (const QString& toolID : toolFactory.availableTabs()){
        ToolTab* tab = toolFactory.create(toolID, m_sharedBuffer);
        if (!tab) {
            qWarning() << "ToolsTabWidget ctor: failed to create tab for id" << toolID;
            continue;
        }

        qDebug() << "ToolsTabWidget ctor: created tab id=" << toolID << " ptr=" << tab << " name=" << tab->toolName();

        tab->setFile(path);
        tab->setProperty("tabDataLoaded", false);

        connect(tab, &ToolTab::refreshDataAllTabsSignal, this, &ToolsTabWidget::refreshDataAllTabs);
        connect(tab, &ToolTab::modifyData, this, &ToolsTabWidget::setupStar);
        connect(tab, &ToolTab::dataEqual, this, &ToolsTabWidget::removeStar);

        this->addTab(tab, tab->toolIcon(), tab->toolName());
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
