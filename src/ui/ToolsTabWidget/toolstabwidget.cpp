#include <QFile>
#include <QTabBar>
#include <utility>
#include <qboxlayout.h>
#include <qfileinfo.h>

#include "ui/ToolsTabWidget/ToolTab.h"
#include "core/file/FileDataBuffer.h"
#include "core/ToolsRegistry.h"
#include "toolstabwidget.h"

ToolsTabWidget::ToolsTabWidget(QWidget *parent, QString path)
    : QTabWidget(parent)
    , m_filePath(std::move(path))
{
    setTabsClosable(true);


    // Создаем общий буфер данных для всех вкладок
    m_sharedBuffer = new FileDataBuffer(this);

    m_sharedBuffer->openFile(m_filePath);

    for (const auto& descriptor : ToolsRegistry::instance().availableFileTools()) {
        if (descriptor.autoOpen) {
            createToolTab(descriptor.id);
        }
    }

    if (this->count() > 0) {
        ToolTab* tab = dynamic_cast<ToolTab*>(this->widget(0));
        if (tab) {
            qDebug() << "ToolsTabWidget ctor: preloading first tab" << tab << tab->name();
            tab->updateData();
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
            qDebug() << "ToolsTabWidget currentChanged: loading tab index=" << index << " ptr=" << tab << " name=" << tab->name();
            tab->updateData();
            tab->setProperty("tabDataLoaded", true);
        }

        QString lastInfo = tab->property("lastStatusBarInfo").toString();
        emit statusBarInfoChanged(lastInfo);
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
    const auto descriptor = ToolsRegistry::instance().descriptor(toolId);
    if (!descriptor.isValid() || descriptor.kind != ToolKind::FileTab) {
        qWarning() << "ToolsTabWidget: unknown tool tab id" << toolId;
        return nullptr;
    }

    ToolTab* tab = ToolsRegistry::instance().createFileTool(toolId, m_sharedBuffer);
    if (!tab) {
        qWarning() << "ToolsTabWidget: failed to create tab for id" << toolId;
        return nullptr;
    }

    tab->setFile(m_filePath);
    tab->setProperty("toolTabId", descriptor.id);
    tab->setProperty("toolTabOrder", descriptor.order);
    tab->setProperty("toolTabClosable", descriptor.fileGroup == FileToolGroup::Other);
    tab->setProperty("tabDataLoaded", false);

    connect(tab, &ToolTab::modifyData, this, &ToolsTabWidget::setupStar);
    connect(tab, &ToolTab::dataEqual, this, &ToolsTabWidget::removeStar);
    connect(tab, &ToolTab::statusBarInfoChanged, this, [this, tab](const QString& info) {
        tab->setProperty("lastStatusBarInfo", info);
        if (currentWidget() == tab)
            emit statusBarInfoChanged(info);
    });

    connect(this, &ToolsTabWidget::setWordWrapSignal, tab, &ToolTab::setWordWrapSlot);
    connect(this, &ToolsTabWidget::setTabReplaceSignal, tab, &ToolTab::setTabReplaceSlot);
    connect(this, &ToolsTabWidget::setTabWidthSignal, tab, &ToolTab::setTabWidthSlot);

    connect(this, &ToolsTabWidget::setWordWrapSignal, tab, &ToolTab::setWordWrapSlot);
    connect(this, &ToolsTabWidget::setTabReplaceSignal, tab, &ToolTab::setTabReplaceSlot);
    connect(this, &ToolsTabWidget::setTabWidthSignal, tab, &ToolTab::setTabWidthSlot);

    int insertIndex = count();
    if (descriptor.fileGroup == FileToolGroup::Always) {
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

    insertTab(insertIndex, tab, tab->icon(), tab->name());
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
        tab->updateData();
        tab->setProperty("tabDataLoaded", true);
    }

    if (activate) {
        setCurrentWidget(tab);
    }

    return tab;
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
    if (m_sharedBuffer && m_sharedBuffer->isModified()) {
        m_sharedBuffer->saveToFile(m_filePath);
    }
}

void ToolsTabWidget::removeStar(){
    for (int tabIndex = 0; tabIndex < count(); ++tabIndex) {
        ToolTab* tab = qobject_cast<ToolTab*>(widget(tabIndex));
        if (tab && tab->getModifyIndicator()) {
            return;
        }
    }

        emit removeStarSignal();
}

void ToolsTabWidget::setupStar(){
    emit setupStarSignal();
}

void ToolsTabWidget::setWordWrapSlot(bool checked){
    qDebug("signal: word wrap");
    emit setWordWrapSignal(checked);
}

void ToolsTabWidget::setTabReplaceSlot(bool checked){
    qDebug("signal: tab replace");
    emit setTabReplaceSignal(checked);
}

void ToolsTabWidget::setTabWidthSlot(int width){
    qDebug("signal: tab width");
    emit setTabWidthSignal(width);
}