#include "idewindow.h"
#include "dialogs/filecreatedialog.h"
#include "QFileSystemModel"
#include "QMessageBox"
#include <qheaderview.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <QApplication>
#include "globalwidgetsmanager.h"
#include "projectshistorymanager.h"
#include "app/WelcomeWindow/welcomeform.h"
#include "dialogs/settingsdialog.h"
#include "dialogs/reversecalculatordialog.h"

IDEWindow::IDEWindow(QString ProjectPath, QWidget *parent)
    : QMainWindow(parent)
{

    // - - Window Settings - -
    this->setWindowState(Qt::WindowMaximized);
    this->setWindowTitle("Cremniy");

    // Save Project In History
    SaveProjectInCache(ProjectPath);

    // - - Main Menu - -
    m_menuBar = menuBar();
    m_fileMenu = m_menuBar->addMenu("File");
    m_editMenu = m_menuBar->addMenu("Edit");
    m_viewMenu = m_menuBar->addMenu("View");
    m_toolsMenu = m_menuBar->addMenu("Tools");
    m_referencesMenu = m_menuBar->addMenu("References");
    m_gitMenu = m_menuBar->addMenu("Git");

    // - - File Menu - -
    m_file_openProject = new QAction("New Project", this);
    m_file_newProject = new QAction("Open Project", this);
    m_file_saveFile = new QAction("Save File", this);
    GlobalWidgetsManager::instance().set_IDEWindow_menuBar_file_saveFile(m_file_saveFile);
    m_file_closeProject = new QAction("Close Project", this);

    // - - View Menu - -
    m_view_wordWrap = new QAction("Word Wrap", this);
    m_view_wordWrap->setCheckable(true);
    m_view_wordWrap->setChecked(true);
    GlobalWidgetsManager::instance().set_IDEWindow_menuBar_view_wordWrap(m_view_wordWrap);

    // - - Tools Menu - -
    m_tools_reverseCalculator = new QAction("Reverse Calculator", this);

    // - - References Menu - -
    m_references_asciiChars = new QAction("ASCII characters", this);
    m_references_keybScancodes = new QAction("Keyboard Scancodes", this);

    // - - Edit Menu - -
    m_edit_settings = new QAction("Settings", this);

    // - - Git Menu - -
    m_git_commit = new QAction("Commit", this);
    m_git_commitAndPush = new QAction("Commit And Push", this);
    m_git_setBranch = new QAction("Set Branch", this);


    // - - Widgets - -
    m_statusBar = statusBar();

    m_mainWidget = new QWidget(this);
    m_mainLayout = new QHBoxLayout(m_mainWidget);
    m_mainSplitter = new QSplitter(m_mainWidget);

    QWidget* leftWidget = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0,0,0,0);
    leftWidget->setLayout(leftLayout);

    m_filesTabWidget = new FilesTabWidget();
    m_filesTreeView = new FileTreeView();

    leftLayout->addWidget(m_filesTreeView);

    // - - Add Actions in Menu Bar - -

    // - File Menu -
    m_fileMenu->addAction(m_file_openProject);
    m_fileMenu->addAction(m_file_newProject);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_file_saveFile);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_file_closeProject);

    // - Edit Menu -
    m_toolsMenu->addSeparator();
    m_editMenu->addAction(m_edit_settings);

    // - View Menu -
    m_viewMenu->addAction(m_view_wordWrap);

    // - Tools Menu -
    m_toolsMenu->addAction(m_tools_reverseCalculator);
    m_toolsMenu->addSeparator();

    // - References Menu -
    m_referencesMenu->addAction(m_references_asciiChars);
    m_referencesMenu->addAction(m_references_keybScancodes);

    // - Git Menu -
    m_gitMenu->addAction(m_git_commit);
    m_gitMenu->addAction(m_git_commitAndPush);
    m_gitMenu->addSeparator();
    m_gitMenu->addAction(m_git_setBranch);

    // - - Tunning Widgets/Layouts - -

    setCentralWidget(m_mainWidget);

    m_mainLayout->addWidget(m_mainSplitter);

    m_filesTabWidget->setObjectName("filesTabWidget");

    m_mainSplitter->addWidget(leftWidget);
    m_mainSplitter->addWidget(m_filesTabWidget);

    m_mainSplitter->setSizes({200, 1000});
    m_mainSplitter->setCollapsible(0, false);
    m_mainSplitter->setCollapsible(1, false);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);

    m_filesTreeView->setMinimumWidth(180);
    m_filesTreeView->setTextElideMode(Qt::ElideNone);
    m_filesTreeView->setIndentation(12);

    QFileSystemModel *model = new QFileSystemModel(this);

    model->setRootPath(ProjectPath);

    model->setReadOnly(false);
    m_filesTreeView->setModel(model);

    // ограничиваем отображение только этой директории
    m_filesTreeView->setRootIndex(model->index(ProjectPath));
    // model->setIconProvider(new IconProvider());

    m_filesTreeView->setColumnHidden(1, true);
    m_filesTreeView->setColumnHidden(2, true);
    m_filesTreeView->setColumnHidden(3, true);
    m_filesTreeView->header()->hide();
    m_filesTreeView->setAnimated(true);
    m_filesTreeView->setEditTriggers(QAbstractItemView::EditKeyPressed);
    m_filesTreeView->setContextMenuPolicy(Qt::CustomContextMenu);

    m_file_saveFile->setShortcut(QKeySequence::Save);

    m_mainLayout->setContentsMargins(0,0,0,0);

    while (m_filesTabWidget->count() > 0) {
        m_filesTabWidget->removeTab(0);
    }

    m_filesTabWidget->setTabsClosable(true);
    m_filesTabWidget->setMovable(true);

    // - - Connects - -

    connect(m_file_closeProject, &QAction::triggered, this, &IDEWindow::on_ClosingProject);
    connect(m_filesTabWidget, &QTabWidget::tabCloseRequested,
            this, [=](int index){
                m_filesTabWidget->removeTab(index);
            });
    connect(m_filesTreeView, &QTreeView::customContextMenuRequested,this, &IDEWindow::on_Tree_ContextMenu);
    connect(m_edit_settings, &QAction::triggered, this, &IDEWindow::on_Open_Settings);
    connect(m_tools_reverseCalculator, &QAction::triggered, this, &IDEWindow::on_Open_ReverseCalculator);
    connect(m_filesTreeView, &QTreeView::doubleClicked, this, &IDEWindow::on_treeView_doubleClicked);
}

IDEWindow::~IDEWindow()
{}

void IDEWindow::on_Open_Settings()
{
    SettingsDialog dlg(this);
    dlg.exec();
}

void IDEWindow::on_Open_ReverseCalculator()
{
    auto *dlg = new ReverseCalculatorDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void IDEWindow::SaveProjectInCache(const QString & project_path){
    utils::ProjectsHistoryManager::saveProjectsHistory(project_path);
}

void IDEWindow::on_ClosingProject() {
    WelcomeForm* wForm = new WelcomeForm();
    wForm->show();

    this->close();
    this->deleteLater();
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
    QModelIndex index = m_filesTreeView->indexAt(pos); // индекс под курсором

    QFileSystemModel *model = qobject_cast<QFileSystemModel*>(m_filesTreeView->model());
    if (!model)
        return;

    QMenu menu(this);

    if (index.isValid()){

        QString path = model->filePath(index);
        QString fileName = model->fileName(index);
        bool isDir = model->isDir(index);  // <-- проверяем, директория ли

        if (isDir){
            menu.addAction("Open", [this, path]() {
                QFileSystemModel *model = qobject_cast<QFileSystemModel*>(m_filesTreeView->model());
                if (!model)
                    return;

                QModelIndex index = model->index(path);
                if (!index.isValid())
                    return;

                // Разворачиваем саму директорию
                m_filesTreeView->expand(index);

                // Прокручиваем и выделяем
                //m_filesTreeView->scrollTo(index);
                //m_filesTreeView->setCurrentIndex(index);
                //m_filesTreeView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

            });

            menu.addAction("Rename", [this, path]() {
                QFileSystemModel *model = qobject_cast<QFileSystemModel*>(m_filesTreeView->model());
                if (!model)
                    return;

                QModelIndex index = model->index(path);
                if (!index.isValid())
                    return;

                // Включаем редактирование индекса
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

                // Включаем редактирование индекса
                m_filesTreeView->edit(index);
            });
            menu.addAction("Delete", [path,this]() {
                QString dialogTitle = QString("Are you sure you want to delete the file \"%1\"?").arg(QFileInfo(path).fileName());
                auto res = QMessageBox::question(this, "Delete", dialogTitle, QMessageBox::Ok | QMessageBox::Cancel);
                if (res == QMessageBox::Ok) QFile(path).remove();
            });
        }

        // Показать меню в глобальных координатах

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

