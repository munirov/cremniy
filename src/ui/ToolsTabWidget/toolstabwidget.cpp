#include <QFile>
#include <QTabBar>
#include <utility>
#include <qboxlayout.h>
#include <qfileinfo.h>

#include "core/file/FileDataBuffer.h"
#include "core/git/gitblameservice.h"
#include "ui/ToolsTabWidget/toolstabwidget.h"
#include "core/modules/ModuleManager.h"
#include "core/modules/TabBase.h"
#include "Modules/Tabs/CodeEditor/codeeditortab.h"

namespace {

QVector<TabGitBlameLineInfo> toTabBlameLines(const QVector<BlameLineInfo>& lines)
{
    QVector<TabGitBlameLineInfo> result;
    result.reserve(lines.size());
    for (const BlameLineInfo& line : lines) {
        result.append({line.authorName,
                       line.authorEmail,
                       line.commitDate,
                       line.shortOid,
                       line.fullOid,
                       line.commitSummary,
                       line.isUncommitted});
    }
    return result;
}

} // namespace

ToolsTabWidget::ToolsTabWidget(QWidget *parent, QString path)
    : QTabWidget(parent)
    , m_filePath(std::move(path))
{
    setTabsClosable(true);


    // Создаем общий буфер данных для всех вкладок
    m_sharedBuffer = new FileDataBuffer(this);
    m_sharedBuffer->openFile(m_filePath);

    createAlwaysTabs();



    if (this->count() > 0) {
        TabBase* tab = dynamic_cast<TabBase*>(this->widget(0));
        if (tab) {
            qDebug() << "ToolsTabWidget ctor: preloading first tab" << tab;
            tab->setTabData();
            tab->setProperty("tabDataLoaded", true);
        }
    }

    connect(
        this,
        &QTabWidget::currentChanged,
        this,
        [this](int index) {
        if (index < 0)
            return;

        TabBase* tab = dynamic_cast<TabBase*>(this->widget(index));
        if (!tab)
            return;

        if (!tab->property("tabDataLoaded").toBool()) {
            qDebug() << "ToolsTabWidget currentChanged: loading tab index=" << index << " ptr=" << tab;
            tab->setTabData();
            tab->setProperty("tabDataLoaded", true);
        }

        QString lastInfo = tab->property("lastStatusBarInfo").toString();
        emit statusBarInfoChanged(lastInfo);
    });

    connect(this,
            &QTabWidget::tabCloseRequested,
            this,
            &ToolsTabWidget::closeToolTab
            );
}

CodeEditorTab* ToolsTabWidget::codeEditorTab(bool activate)
{
    for (int index = 0; index < count(); ++index) {
        auto* editor = qobject_cast<CodeEditorTab*>(widget(index));
        if (!editor)
            continue;
        if (activate)
            setCurrentIndex(index);
        return editor;
    }
    return nullptr;
}

bool ToolsTabWidget::gitBlameEnabled() const
{
    for (int index = 0; index < count(); ++index) {
        const auto* tab = qobject_cast<const TabBase*>(widget(index));
        if (tab && tab->gitBlameEnabled())
            return true;
    }
    return false;
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

void ToolsTabWidget::createAlwaysTabs()
{

    QString m_alwaysVisibleGroup = "always";
    QList<QString> groups = ModuleManager::instance().getGroups<TabBase>();
    if (!groups.contains(m_alwaysVisibleGroup)) return;

    const QVector<ModuleDescription<TabBase>>& tabsDesc = ModuleManager::instance().getByGroup<TabBase>(m_alwaysVisibleGroup);

    for (const ModuleDescription<TabBase>& desc: tabsDesc){
        this->createTab(desc, true, false);
    }
    if (count() > 0) this->setCurrentIndex(0);
}

void ToolsTabWidget::createTab(const ModuleDescription<TabBase>& desc, bool isAlways, bool tabClosable){

    TabBase* tab = desc.creator();
    if (!tab) return;

    connectGitIntegration(tab);
    tab->setFile(m_filePath);
    tab->setFileDataBuffer(m_sharedBuffer);

    tab->setProperty("toolTabOrder", desc.position);
    tab->setProperty("toolTabClosable", tabClosable);
    tab->setProperty("tabDataLoaded", false);

    connect(tab, &TabBase::refreshDataAllTabsSignal, this, &ToolsTabWidget::refreshDataAllTabs);
    connect(tab, &TabBase::modifyData, this, &ToolsTabWidget::setupStar);
    connect(tab, &TabBase::dataEqual, this, &ToolsTabWidget::removeStar);
    connect(tab, &TabBase::statusBarInfoChanged, this, [this, tab](const QString& info) {
        tab->setProperty("lastStatusBarInfo", info);
        if (currentWidget() == tab)
            emit statusBarInfoChanged(info);
    });

    connect(this, &ToolsTabWidget::setWordWrapSignal, tab, &TabBase::setWordWrapSlot);
    connect(this, &ToolsTabWidget::setTabReplaceSignal, tab, &TabBase::setTabReplaceSlot);
    connect(this, &ToolsTabWidget::setTabWidthSignal, tab, &TabBase::setTabWidthSlot);
    connect(this, &ToolsTabWidget::setGitBlameSignal, tab, &TabBase::setGitBlameSlot);
    connect(tab, &TabBase::gitBlameEnabledChanged,
            this, &ToolsTabWidget::gitBlameEnabledChanged);

    int insertIndex = count();

    if (isAlways){
        for (int index = 0; index < count(); ++index) {
            QWidget* existingWidget = widget(index);
            const bool existingClosable = existingWidget->property("toolTabClosable").toBool();
            const int existingOrder = existingWidget->property("toolTabOrder").toInt();
            if (existingClosable || desc.position < existingOrder) {
                insertIndex = index;
                break;
            }
        }
    }

    insertTab(insertIndex, tab, tab->icon(), desc.name());
    updateCloseButtons();
}

void ToolsTabWidget::connectGitIntegration(TabBase* tab)
{
    GitBlameService* service = GitBlameService::instance();

    connect(tab, &TabBase::gitBlameRequested,
            service, &GitBlameService::requestBlame);
    connect(service, &GitBlameService::blameReady,
            tab, [tab](const QString& filePath, const QVector<BlameLineInfo>& lines) {
                tab->setGitBlameData(filePath, toTabBlameLines(lines));
            });
    connect(service, &GitBlameService::blameFailed,
            tab, [tab](const QString& filePath, const QString& error) {
                tab->setGitBlameError(filePath, error);
            });
    connect(service, &GitBlameService::repositoryChanged,
            tab, &TabBase::refreshGitBlame);
}

void ToolsTabWidget::refreshDataAllTabs(){
    for (int tabIndex = 0; tabIndex < this->count(); tabIndex++){
        if (tabIndex != this->currentIndex()){
            TabBase* tab = dynamic_cast<TabBase*>(this->widget(tabIndex));
            tab->setTabData();
        }
    }
}

bool ToolsTabWidget::reloadFromDisk()
{
    if (!m_sharedBuffer)
        return false;

    const QString path = m_sharedBuffer->filePath();
    if (path.isEmpty())
        return false;

    /* Keep the reading position across the reload when possible */
    CodeEditorTab* editor = codeEditorTab(false);
    const qint64 cursorLineBefore = editor ? editor->currentCursorLine() : -1;

    if (!m_sharedBuffer->openFile(path))
        return false;

    for (int tabIndex = 0; tabIndex < this->count(); ++tabIndex){
        TabBase* tab = dynamic_cast<TabBase*>(this->widget(tabIndex));
        if (tab) {
            tab->setTabData();
            tab->setProperty("tabDataLoaded", true);
        }
    }

    if (cursorLineBefore > 0) {
        editor = codeEditorTab(false);
        if (editor)
            editor->goToCursorLine(cursorLineBefore);
    }

    return true;
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
    TabBase* tab = dynamic_cast<TabBase*>(currentWidget());
    if (tab) tab->saveTabData();
}

void ToolsTabWidget::removeStar(){

    int toolCount_WithoutModIndicator = 0;
    for (int tabIndex = 0; tabIndex < this->count(); tabIndex++){
        if (tabIndex != this->currentIndex()){
            TabBase* tab = dynamic_cast<TabBase*>(this->widget(tabIndex));
            if (!tab) {
                qWarning() << "ToolsTabWidget: removeStar(): null tab at index" << tabIndex;
                continue;
            }
            qDebug() << "ToolsTabWidget: removeStar()";
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

    // signal "setup star" to up
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

void ToolsTabWidget::setGitBlameSlot(bool checked)
{
    emit setGitBlameSignal(checked);
}

void ToolsTabWidget::openTabModule(ModuleDescription<TabBase> desc){
    qDebug() << "ToolsTabWidget::openTabModule(): " << desc.name();
    this->createTab(desc);
    if (count() > 0) this->setCurrentIndex(count()-1);
}
