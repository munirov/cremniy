#include "idewindow.h"

#include "QFileSystemModel"
#include "QMessageBox"
#include "app/WelcomeWindow/welcomeform.h"
#include "dialogs/filecreatedialog.h"
#include "dialogs/settingsdialog.h"
#include "ui/MenuBar/menubarbuilder.h"

#include <QApplication>
#include <QAction>
#include <QFileDialog>
#include <QMenu>
#include <QStandardPaths>
#include <QToolBar>
#include <qheaderview.h>
#include <qjsondocument.h>
#include <qjsonobject.h>

#include "projectshistorymanager.h"

IDEWindow::IDEWindow(QString ProjectPath, QWidget *parent)
    : QMainWindow(parent)
{
    this->setWindowState(Qt::WindowMaximized);
    this->setWindowTitle("Cremniy");

    MenuBarBuilder* menuBarBuilder = new MenuBarBuilder(menuBar(), this);
    Q_UNUSED(menuBarBuilder);
    menuBar()->setNativeMenuBar(false);

    SaveProjectInCache(ProjectPath);

    m_buildManager = new BuildManager(this);

    QToolBar* buildBar = addToolBar("Build");
    buildBar->setObjectName("BuildToolbar");
    auto* actBuild = buildBar->addAction("▶ Build");
    auto* actRun = buildBar->addAction("⏵ Run");
    auto* actClean = buildBar->addAction("Clean");
    auto* actStop = buildBar->addAction("Stop");

    connect(actBuild, &QAction::triggered, m_buildManager, &BuildManager::runBuild);
    connect(actRun, &QAction::triggered, m_buildManager, &BuildManager::runRun);
    connect(actClean, &QAction::triggered, m_buildManager, &BuildManager::runClean);
    connect(actStop, &QAction::triggered, m_buildManager, &BuildManager::stopProcess);

    QAction* configureBuildAction = nullptr;
    for (QAction* menuAction : menuBar()->actions()) {
        QMenu* menu = menuAction->menu();
        if (!menu)
            continue;
        if (menu->title() == "Tools") {
            menu->addSeparator();
            configureBuildAction = menu->addAction("Configure Build");
            break;
        }
    }
    m_statusBar = statusBar();

    m_mainWidget = new QWidget(this);
    m_mainLayout = new QHBoxLayout(m_mainWidget);
    m_mainLayout->setContentsMargins(0,0,0,0);

    m_mainSplitter = new QSplitter(Qt::Horizontal, m_mainWidget);
    m_verticalSplitter = new QSplitter(Qt::Vertical, m_mainWidget);
    m_terminal = new TerminalWidget(this);

    QWidget* leftWidget = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0,0,0,0);

    m_filesTabWidget = new FilesTabWidget(this);
    m_filesTabWidget->setObjectName("filesTabWidget");
    m_filesTreeView = new FileTreeView();
    leftLayout->addWidget(m_filesTreeView);

    m_mainSplitter->addWidget(leftWidget);
    m_mainSplitter->addWidget(m_filesTabWidget);
    m_mainSplitter->setSizes({200, 1000});

    m_verticalSplitter->addWidget(m_mainSplitter);
    m_verticalSplitter->addWidget(m_terminal);
    m_verticalSplitter->setSizes({800, 200});

    connect(m_buildManager, &BuildManager::outputLine,
            m_terminal, &TerminalWidget::appendLine);
    connect(m_buildManager, &BuildManager::processStarted,
            this, [this](const QString& cmd) {
                m_terminal->appendLine(QString());
                m_terminal->appendLine(QString("=== %1 ===").arg(cmd));
            });
    connect(m_buildManager, &BuildManager::processFinished,
            this, [this](int code) {
            m_terminal->appendLine(
                QString("=== Finished (exit code %1) ===").arg(code));
            });
    connect(m_buildManager, &BuildManager::errorOccurred,
            m_terminal, &TerminalWidget::appendLine);

    m_mainLayout->addWidget(m_verticalSplitter);
    setCentralWidget(m_mainWidget);

    leftLayout->addWidget(m_filesTreeView);
    m_mainSplitter->setSizes({200, 1000});
    m_mainSplitter->setCollapsible(0, false);
    m_mainSplitter->setCollapsible(1, false);

    m_verticalSplitter->setSizes({800, 200});
    
    if (m_verticalSplitter->count() > 1) {
        m_verticalSplitter->setCollapsible(1, true);
    }

    m_filesTreeView->setMinimumWidth(180);
    m_filesTreeView->setTextElideMode(Qt::ElideNone);
    m_filesTreeView->setIndentation(12);

    QFileSystemModel *model = new QFileSystemModel(this);
    model->setRootPath(ProjectPath);
    model->setReadOnly(false);
    m_filesTreeView->setModel(model);
    m_filesTreeView->setRootIndex(model->index(ProjectPath));

    m_filesTreeView->setColumnHidden(1, true);
    m_filesTreeView->setColumnHidden(2, true);
    m_filesTreeView->setColumnHidden(3, true);
    m_filesTreeView->header()->hide();
    m_filesTreeView->setAnimated(true);
    m_filesTreeView->setEditTriggers(QAbstractItemView::EditKeyPressed);
    m_filesTreeView->setContextMenuPolicy(Qt::CustomContextMenu);

    while (m_filesTabWidget->count() > 0) {
        m_filesTabWidget->removeTab(0);
    }

    m_filesTabWidget->setTabsClosable(true);
    m_filesTabWidget->setMovable(true);

    connect(this, &IDEWindow::saveFileSignal, m_filesTabWidget, &FilesTabWidget::saveFileSlot);
    connect(m_filesTabWidget, &QTabWidget::tabCloseRequested,
            m_filesTabWidget, &FilesTabWidget::closeTab);
    connect(m_filesTreeView, &QTreeView::customContextMenuRequested, this, &IDEWindow::on_Tree_ContextMenu);
    connect(m_filesTreeView, &QTreeView::doubleClicked, this, &IDEWindow::on_treeView_doubleClicked);

    if (configureBuildAction) {
        connect(configureBuildAction, &QAction::triggered, this, [this]() {
            BuildConfig current = m_buildManager->config();
            BuildSetupDialog dlg(current, this);
            dlg.setWindowTitle("Configure Build");
            if (dlg.exec() == QDialog::Accepted) {
                BuildConfig cfg = dlg.result();
                BuildConfigManager::save(m_projectDir, cfg);
                m_buildManager->setConfig(cfg);
                m_terminal->appendLine("Build config updated -> " + m_projectDir + "/cremniy.json");
            }
        });
    }

    onProjectOpened(ProjectPath);
}

IDEWindow::~IDEWindow()
{}

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

    if(checked) {
        m_terminal->setFocus();
    }
}

void IDEWindow::on_ClosingProject() {
    emit CloseProject();
    this->close();
}

void IDEWindow::on_treeView_doubleClicked(const QModelIndex &index)
{
    auto *model = static_cast<QFileSystemModel*>(m_filesTreeView->model());
    if (model->isDir(index)) return;
    QString fileName = model->fileName(index);
    QString filePath = model->filePath(index);

    m_filesTabWidget->openFile(filePath, fileName);
}

void IDEWindow::on_Tree_ContextMenu(const QPoint &pos)
{
    QModelIndex index = m_filesTreeView->indexAt(pos);

    QFileSystemModel *model = qobject_cast<QFileSystemModel*>(m_filesTreeView->model());
    if (!model)
        return;

    QMenu menu(this);

    if (index.isValid()){
        QString path = model->filePath(index);
        QString fileName = model->fileName(index);
        bool isDir = model->isDir(index);

        if (isDir){
            menu.addAction("Open", [this, path]() {
                QFileSystemModel *model = qobject_cast<QFileSystemModel*>(m_filesTreeView->model());
                if (!model)
                    return;

                QModelIndex index = model->index(path);
                if (!index.isValid())
                    return;

                m_filesTreeView->expand(index);
            });

            menu.addAction("Rename", [this, path]() {
                QFileSystemModel *model = qobject_cast<QFileSystemModel*>(m_filesTreeView->model());
                if (!model)
                    return;

                QModelIndex index = model->index(path);
                if (!index.isValid())
                    return;

                m_filesTreeView->edit(index);
            });
            menu.addAction("Delete", [path, this]() {
                QDir dir(path);
                QString dialogTitle = QString("Are you sure you want to delete the folder \"%1\"?").arg(dir.dirName());
                auto res = QMessageBox::question(this, "Delete", dialogTitle, QMessageBox::Ok | QMessageBox::Cancel);
                if (res == QMessageBox::Ok) dir.removeRecursively();
            });
            menu.addSeparator();
            menu.addAction("Create File", [path,this]() {
                FileCreateDialog fcd(this,path,false);
                fcd.exec();
            });
            menu.addAction("Create Folder", [path,this]() {
                FileCreateDialog fcd(this,path,true);
                fcd.exec();
            });
        }
        else{
            menu.addAction("Open", [this, path, fileName]() {
                m_filesTabWidget->openFile(path, fileName);
            });
            menu.addAction("Rename", [this, path]() {
                QFileSystemModel *model = qobject_cast<QFileSystemModel*>(m_filesTreeView->model());
                if (!model)
                    return;

                QModelIndex index = model->index(path);
                if (!index.isValid())
                    return;

                m_filesTreeView->edit(index);
            });
            menu.addAction("Delete", [path,this]() {
                QString dialogTitle = QString("Are you sure you want to delete the file \"%1\"?").arg(QFileInfo(path).fileName());
                auto res = QMessageBox::question(this, "Delete", dialogTitle, QMessageBox::Ok | QMessageBox::Cancel);
                if (res == QMessageBox::Ok) QFile(path).remove();
            });
        }
    }
    else{
        QString path = model->rootPath();
        menu.addAction("Create File", [path,this]() {
            FileCreateDialog fcd(this,path,false);
            fcd.exec();
        });
        menu.addAction("Create Folder", [path,this]() {
            FileCreateDialog fcd(this,path,true);
            fcd.exec();
        });
    }
    menu.exec(m_filesTreeView->viewport()->mapToGlobal(pos));
}

void IDEWindow::onProjectOpened(const QString& projectDir)
{
    m_projectDir = projectDir;
    m_buildManager->setProjectDir(projectDir);

    BuildConfig cfg;
    if (BuildConfigManager::load(projectDir, cfg)) {
        m_buildManager->setConfig(cfg);
        m_terminal->appendLine("Loaded build config from " + projectDir + "/cremniy.json");
        return;
    }

    BuildSetupDialog* dlg = nullptr;
    if (BuildConfigManager::autoDetect(projectDir, cfg)) {
        dlg = new BuildSetupDialog(cfg, this);
        dlg->setWindowTitle("Build system detected - confirm settings");
    } else {
        dlg = new BuildSetupDialog({}, this);
        dlg->setWindowTitle("Configure build commands");
    }

    if (dlg->exec() == QDialog::Accepted) {
        cfg = dlg->result();
        BuildConfigManager::save(projectDir, cfg);
        m_buildManager->setConfig(cfg);
        m_terminal->appendLine("Build config saved -> " + projectDir + "/cremniy.json");
    } else {
        m_terminal->appendLine("Build config not set. Use Tools -> Configure Build to set it up.");
    }
    delete dlg;
}

void IDEWindow::SaveProjectInCache(const QString project_path)
{
    utils::ProjectsHistoryManager::saveProjectsHistory(project_path);
}

void IDEWindow::on_NewProject()
{
}

void IDEWindow::on_OpenProject()
{
}

void IDEWindow::on_SaveFile()
{
    qDebug() << "IDEWindow::on_SaveFile()";
    emit saveFileSignal();
}

void IDEWindow::on_openSettings()
{
    SettingsDialog dlg(this);
    dlg.exec();
}
