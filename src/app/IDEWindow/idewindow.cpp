#include "idewindow.h"
#include "dialogs/filecreatedialog.h"
#include "Modules/Tabs/CodeEditor/search/SearchPanelWidget.h"
#include "Modules/Tabs/CodeEditor/codeeditortab.h"
#include "QFileSystemModel"
#include "QMessageBox"
#include <qheaderview.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include "core/icons/iconprovider.h"
#include <QApplication>
#include "dialogs/settingsdialog.h"
#include "ui/MenuBar/menubarbuilder.h"

IDEWindow::IDEWindow(const QString &ProjectPath, QWidget *parent)
    : QMainWindow(parent), m_projectPath(ProjectPath) {
    // - - Window Settings - -
    this->setWindowState(Qt::WindowMaximized);
    this->setWindowTitle("Cremniy");

    // - - Menu Bar - -
    auto const menu = menuBar();
    MenuBarBuilder const *menuBarBuilder = new MenuBarBuilder(menu, this);
    menu->setNativeMenuBar(false);

    // - - Widgets - -
    m_statusBar = statusBar();
    m_statusLabel = new QLabel(this);
    m_statusBar->addPermanentWidget(m_statusLabel);

    m_mainWidget = new QWidget(this);
    m_mainLayout = new QHBoxLayout(m_mainWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    m_mainSplitter = new QSplitter(Qt::Horizontal, m_mainWidget);

    m_verticalSplitter = new QSplitter(Qt::Vertical, m_mainWidget);

    // Terminal is initialized lazily on demand (see on_Toggle_Terminal)
    m_terminal = nullptr;

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
    m_mainSplitter->setSizes({200, 1000});

    m_verticalSplitter->addWidget(m_mainSplitter);
    m_verticalSplitter->setSizes({800, 200});

    m_mainLayout->addWidget(m_verticalSplitter);
    setCentralWidget(m_mainWidget);

    // - - Right Sidebar (Search) - -
    m_rightSidebar = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(m_rightSidebar);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_searchPanel = new SearchPanelWidget(nullptr, m_rightSidebar);
    rightLayout->addWidget(m_searchPanel);
    m_mainSplitter->addWidget(m_rightSidebar);
    m_rightSidebar->hide();

    // - - Tunning Widgets/Layouts - -
    m_mainSplitter->setSizes({200, 800, 300});
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
    connect(this, &IDEWindow::openTabModule, m_filesTabWidget, &FilesTabWidget::openTabModule);

    connect(m_filesTreeView, &FileTreePanel::openFileRequested, this, [this](const QString& filePath, const QString& fileName) {
            m_filesTabWidget->openFile(filePath, fileName);
    });

    // Search dock connections
    connect(m_filesTabWidget, &FilesTabWidget::findRequested, this, &IDEWindow::on_FindInFile);
    connect(m_filesTabWidget, &FilesTabWidget::projectFindRequested, this, &IDEWindow::on_FindInProject);

    connect(m_searchPanel, &SearchPanelWidget::openFileAtLine,
            m_filesTabWidget, &FilesTabWidget::openFileAtLine);

    // Update search panel's editor reference when tab changes
    connect(m_filesTabWidget, &QTabWidget::currentChanged, this, [this](int) {
        updateSearchEditor();
    });
}

IDEWindow::~IDEWindow() = default;

void IDEWindow::updateSearchEditor()
{
    if (!m_searchPanel)
        return;

    if (auto* fileTab = qobject_cast<FileTab*>(m_filesTabWidget->currentWidget())) {
        if (auto* tools = fileTab->toolsTabWidget()) {
            for (int i = 0; i < tools->count(); ++i) {
                if (auto* codeTab = qobject_cast<CodeEditorTab*>(tools->widget(i))) {
                    m_searchPanel->setEditor(codeTab->editor());
                    return;
                }
            }
        }
    }
    m_searchPanel->setEditor(nullptr);
}

void IDEWindow::on_Toggle_Terminal(bool checked) {
    if (checked && !m_terminal) {
        m_terminal = new TerminalWidget(this);
        m_verticalSplitter->addWidget(m_terminal);
        m_verticalSplitter->setCollapsible(1, true);
        m_verticalSplitter->setSizes({800, 200});
    }

    if (!m_terminal) {
        return;
    }

    m_terminal->setVisible(checked);

    if (checked) {
        m_terminal->setFocus();
    }
}

void IDEWindow::on_Toggle_Search(bool checked) {
    m_rightSidebar->setVisible(checked);
    if (checked) {
        updateSearchEditor();
        m_searchPanel->setFocus();
    }
}

void IDEWindow::on_FindInFile() {
    if (m_rightSidebar->isVisible() && m_searchMode == 0) {
        m_rightSidebar->hide();
        m_searchMode = -1;
        return;
    }
    m_rightSidebar->show();
    updateSearchEditor();
    m_searchPanel->showInFileMode();
    m_searchMode = 0;
}

void IDEWindow::on_FindInProject() {
    if (m_rightSidebar->isVisible() && m_searchMode == 1) {
        m_rightSidebar->hide();
        m_searchMode = -1;
        return;
    }
    m_rightSidebar->show();
    updateSearchEditor();
    m_searchPanel->showProjectMode();
    m_searchMode = 1;
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
