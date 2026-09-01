#include "idewindow.h"
#include "QFileSystemModel"
#include "QMessageBox"
#include <qheaderview.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <QApplication>
#include "dialogs/configurebuild.h"
#include "dialogs/settingsdialog.h"
#include "ui/MenuBar/menubarbuilder.h"
#include "widgets/search/searchpanel.h"
#include "widgets/terminal/terminalpanel.h"
#include <QShortcut>
#include <qtimer.h>

IDEWindow::IDEWindow(const QString &ProjectPath, QWidget *parent)
    : QMainWindow(parent), m_projectPath(ProjectPath) {
    setProperty("projectPath", ProjectPath);
    // - - Window Settings - -
    this->setWindowState(Qt::WindowMaximized);
    this->setWindowTitle("Cremniy");
    setMinimumSize(800, 600);

    // - - Menu Bar - -
    auto const menu = menuBar();
    MenuBarBuilder menuBarBuilder(menu, this);
    menu->setNativeMenuBar(false);

    // - - Get Project Info - -
    if (!ProjectInfoManager::loadProjectInfo(ProjectPath, m_projectInfo)){
        QString dirName = QDir(ProjectPath).dirName();
        m_projectInfo.name = dirName;
        m_projectInfo.path = ProjectPath;
        ProjectInfoManager::saveProjectInfo(m_projectInfo);
    }

    // - - Widgets - -
    m_statusBar = statusBar();
    m_statusLabel = new QLabel(this);
    m_statusBar->addPermanentWidget(m_statusLabel);

    m_mainWidget = new QWidget(this);
    m_mainLayout = new QHBoxLayout(m_mainWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    m_mainSplitter = new QSplitter(Qt::Horizontal, m_mainWidget);

    m_verticalSplitter = new QSplitter(Qt::Vertical, m_mainWidget);

    m_terminalPanel = nullptr;

    m_leftSidebar = new QWidget(this);
    auto const leftLayout = new QVBoxLayout(m_leftSidebar);

    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_filesTabWidget = new FilesTabWidget(this);
    m_filesTabWidget->setObjectName("filesTabWidget");

    const auto model = new QFileSystemModel();
    const auto proxy = new ExclusionFilterProxyModel();

    m_filesTreeView = new FileTreePanel(this, model, proxy, ProjectPath);
    leftLayout->addWidget(m_filesTreeView, 1);

    m_mainSplitter->addWidget(m_leftSidebar);
    m_mainSplitter->addWidget(m_filesTabWidget);
    m_searchPanel = new SearchPanel(ProjectPath, m_filesTabWidget, m_mainSplitter);
    m_mainSplitter->addWidget(m_searchPanel);
    m_searchPanel->hide();
    m_mainSplitter->setSizes({200, 1000, 0});
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);

    m_verticalSplitter->addWidget(m_mainSplitter); // Сверху все наше IDE
    m_verticalSplitter->setSizes({800, 200});

    m_mainLayout->addWidget(m_verticalSplitter);
    setCentralWidget(m_mainWidget);


    // - - Tunning Widgets/Layouts - -
    m_mainSplitter->setSizes({200, 1000, 0});
    m_mainSplitter->setCollapsible(0, false);
    m_mainSplitter->setCollapsible(1, false);
    m_mainSplitter->setCollapsible(2, true);

    m_verticalSplitter->setSizes({800, 200});

    if (m_verticalSplitter->count() > 1) {
        m_verticalSplitter->setCollapsible(1, true);
    }

    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    while (m_filesTabWidget->count() > 0) {
        m_filesTabWidget->removeTab(0);
    }

    m_filesTabWidget->setTabsClosable(true);
    m_filesTabWidget->setMovable(true);

    // - - Connects - -

    connect(this, &IDEWindow::saveFileSignal, m_filesTabWidget, &FilesTabWidget::saveFileSlot);

    connect(m_filesTabWidget, &FilesTabWidget::statusBarInfoChanged,this, [this](const QString &info) {
        m_statusLabel->setText(info);
    });

    connect(m_filesTabWidget, &QTabWidget::tabCloseRequested, m_filesTabWidget, &FilesTabWidget::closeTab);
    connect(this, &IDEWindow::setWordWrapSignal, m_filesTabWidget, &FilesTabWidget::setWordWrapSlot);
    connect(this, &IDEWindow::setTabReplaceSignal, m_filesTabWidget, &FilesTabWidget::setTabReplaceSlot);
    connect(this, &IDEWindow::setTabWidthSignal, m_filesTabWidget, &FilesTabWidget::setTabWidthSlot);
    connect(this, &IDEWindow::setGitBlameSignal, m_filesTabWidget, &FilesTabWidget::setGitBlameSlot);
    connect(m_filesTabWidget, &FilesTabWidget::gitBlameEnabledChanged,
            this, &IDEWindow::gitBlameEnabledChanged);
    connect(this, &IDEWindow::openTabModule, m_filesTabWidget, &FilesTabWidget::openTabModule);

    connect(m_filesTreeView, &FileTreePanel::openFileRequested, this, [this](const QString& filePath, const QString& fileName) {
            m_filesTabWidget->openFile(filePath, fileName);
    });
    m_closeSearchShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    m_closeSearchShortcut->setContext(Qt::WindowShortcut);
    m_closeSearchShortcut->setEnabled(false);
    const auto closeSearch = [this] {
        m_searchPanel->hide();
        m_closeSearchShortcut->setEnabled(false);
        if (auto* tab = currentFileTab())
            tab->setFocus(Qt::ShortcutFocusReason);
    };
    connect(m_searchPanel, &SearchPanel::closeRequested, this, closeSearch);
    connect(m_closeSearchShortcut, &QShortcut::activated, this, closeSearch);

    auto* nextSearchResult = new QShortcut(QKeySequence(Qt::Key_F3), this);
    nextSearchResult->setContext(Qt::WindowShortcut);
    connect(nextSearchResult, &QShortcut::activated, this, [this] {
        if (m_searchPanel->isVisible())
            m_searchPanel->nextResult();
        else
            on_Find();
    });
    auto* previousSearchResult = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3), this);
    previousSearchResult->setContext(Qt::WindowShortcut);
    connect(previousSearchResult, &QShortcut::activated, this, [this] {
        if (m_searchPanel->isVisible())
            m_searchPanel->previousResult();
        else
            on_Find();
    });
    connect(m_searchPanel, &SearchPanel::statusMessage, this, [this](const QString& message) {
        m_statusLabel->setText(message);
    });

    // - - Configure Build - -
    QTimer::singleShot(0, this, &IDEWindow::configurateBuild);

}

IDEWindow::~IDEWindow() = default;

bool IDEWindow::gitBlameEnabled() const
{
    return m_filesTabWidget && m_filesTabWidget->gitBlameEnabled();
}

void IDEWindow::configurateBuild(){

    if (m_projectInfo.buildCommand.trimmed().isEmpty()){
        openBuildConfigurate();
    }

}


void IDEWindow::openBuildConfigurate(){

    ConfigureBuild confBuildDialog(m_projectInfo, this);
    if (confBuildDialog.exec() == QDialog::Accepted) {
        ProjectInfoManager::saveProjectInfo(m_projectInfo);
    }

}


void IDEWindow::on_Build(){
    m_filesTabWidget->createBuildTab(m_projectInfo);
}


void IDEWindow::on_openBuildConfigurate(){
    openBuildConfigurate();
}


void IDEWindow::on_Toggle_Terminal(bool checked) {
    if (checked && !m_terminalPanel) {
        auto *panel = new TerminalPanel(m_projectPath, this);
        m_terminalPanel = panel;
        m_verticalSplitter->addWidget(panel);
        m_verticalSplitter->setCollapsible(1, true);
        m_verticalSplitter->setSizes({800, 200});

        connect(panel, &TerminalPanel::closeRequested, this, [this, panel] {
            if (m_terminalPanel != panel)
                return;

            m_terminalPanel = nullptr;
            panel->hide();
            panel->deleteLater();
            emit terminalVisibilityChanged(false);

            if (auto *tab = currentFileTab())
                tab->setFocus(Qt::ShortcutFocusReason);
        });
    }

    if (!m_terminalPanel) {
        emit terminalVisibilityChanged(false);
        return;
    }

    m_terminalPanel->setVisible(checked);
    emit terminalVisibilityChanged(checked);

    if (checked) {
        m_terminalPanel->focusActiveTerminal();
    }
}

void IDEWindow::on_SetWordWrap(bool checked) {
    emit setWordWrapSignal(checked);
}

void IDEWindow::on_SetTabReplace(bool checked) {
    emit setTabReplaceSignal(checked);
}

void IDEWindow::on_SetTabWidth(int width) {
    emit setTabWidthSignal(width);
}

void IDEWindow::on_SetGitBlame(bool enabled)
{
    emit setGitBlameSignal(enabled);
}

void IDEWindow::on_Toggle_FileTree(bool checked) const {
    m_leftSidebar->setVisible(checked);
}

void IDEWindow::on_ClosingProject() {
    emit CloseProject();
    this->close();
}

void IDEWindow::on_NewProject() {
}

void IDEWindow::on_OpenProject() {
}

void IDEWindow::on_SaveFile() {
    qDebug() << "IDEWindow::on_SaveFile()";
    emit saveFileSignal();
}

void IDEWindow::on_openSettings() {
    SettingsDialog dlg(this);
    dlg.exec();
}

FileTab* IDEWindow::currentFileTab() const
{
    return qobject_cast<FileTab*>(m_filesTabWidget->currentWidget());
}

void IDEWindow::showSearch(SearchScope scope, bool replaceMode)
{
    m_searchPanel->show();
    m_closeSearchShortcut->setEnabled(true);
    const int totalWidth = qMax(1, m_mainSplitter->width());
    const int sidebarWidth = qMax(180, m_leftSidebar->width());
    const int panelWidth = qBound(360, totalWidth / 3, 480);
    const int editorWidth = qMax(320, totalWidth - sidebarWidth - panelWidth);
    m_mainSplitter->setSizes({sidebarWidth, editorWidth, panelWidth});
    m_searchPanel->open(scope, replaceMode, m_filesTabWidget->selectedSearchText());
}

void IDEWindow::on_Find()
{
    showSearch(SearchScope::CurrentFile, false);
}

void IDEWindow::on_FindOpenFiles()
{
    showSearch(SearchScope::OpenFiles, false);
}

void IDEWindow::on_FindInProject()
{
    showSearch(SearchScope::Project, false);
}

void IDEWindow::on_Replace()
{
    showSearch(SearchScope::CurrentFile, true);
}
