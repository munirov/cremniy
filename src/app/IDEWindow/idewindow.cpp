#include "idewindow.h"
#include "dialogs/filecreatedialog.h"
#include "QFileSystemModel"
#include "QMessageBox"
#include <qheaderview.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <QApplication>
#include "dialogs/settingsdialog.h"
#include "ui/MenuBar/menubarbuilder.h"

IDEWindow::IDEWindow(QString ProjectPath, QWidget *parent)
    : QMainWindow(parent)
{

    /* - - Window Settings - - */
    this->setWindowState(Qt::WindowMaximized);
    this->setWindowTitle("Cremniy");

    /* - - Menu Bar - - */
    MenuBarBuilder* menuBarBuilder = new MenuBarBuilder(menuBar(), this);

    menuBar()->setNativeMenuBar(false);
    /* - - Widgets - - */
    m_statusBar = statusBar();

    m_mainWidget = new QWidget(this);
    m_mainLayout = new QHBoxLayout(m_mainWidget);
    m_mainLayout->setContentsMargins(0,0,0,0);

    m_mainSplitter = new QSplitter(Qt::Horizontal, m_mainWidget);

    m_verticalSplitter = new QSplitter(Qt::Vertical, m_mainWidget);

    /* Terminal is initialized lazily on demand (see on_Toggle_Terminal)
    * m_terminal = new TerminalWidget(this, ProjectPath);
    */ m_terminal->setVisible(false);
    m_terminal = nullptr;

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

    m_verticalSplitter->addWidget(m_mainSplitter); /* Entire IDE UI on top */
    m_verticalSplitter->setSizes({800, 200});      /* Initial proportions  */

    m_mainLayout->addWidget(m_verticalSplitter);
    setCentralWidget(m_mainWidget);

    leftLayout->addWidget(m_filesTreeView);

    /* - - Tunning Widgets/Layouts - - */
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

    m_mainLayout->setContentsMargins(0,0,0,0);

    while (m_filesTabWidget->count() > 0) {
        m_filesTabWidget->removeTab(0);
    }

    m_filesTabWidget->setTabsClosable(true);
    m_filesTabWidget->setMovable(true);

    /* - - Connects - - */

    connect(this, &IDEWindow::saveFileSignal, m_filesTabWidget, &FilesTabWidget::saveFileSlot);

    connect(m_filesTabWidget, &QTabWidget::tabCloseRequested,
            m_filesTabWidget, &FilesTabWidget::closeTab);
    connect(m_filesTreeView, &QTreeView::customContextMenuRequested,this, &IDEWindow::on_Tree_ContextMenu);
    connect(m_filesTreeView, &QTreeView::doubleClicked, this, &IDEWindow::on_treeView_doubleClicked);
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

                /* Enable index edit */
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

void IDEWindow::on_NewProject(){

}

void IDEWindow::on_OpenProject(){

}

void IDEWindow::on_SaveFile(){
    qDebug() << "IDEWindow::on_SaveFile()";
    emit saveFileSignal();
}

void IDEWindow::on_openSettings(){
    SettingsDialog dlg(this);
    dlg.exec();
}
