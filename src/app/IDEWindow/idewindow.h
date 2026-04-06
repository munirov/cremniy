#ifndef IDEWINDOW_H
#define IDEWINDOW_H

#include "filestabwidget.h"
#include "filetreeview.h"
#include "widgets/terminal/terminalwidget.h"
#include "BuildSystem/BuildConfig.h"
#include "BuildSystem/BuildManager.h"
#include "BuildSystem/BuildSetupDialog.h"

#include <QMainWindow>
#include <qboxlayout.h>
#include <qmenubar.h>
#include <qsplitter.h>
#include <qstatusbar.h>

class IDEWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit IDEWindow(QString ProjectPath, QWidget *parent = nullptr);
    ~IDEWindow() override;

private slots:
    void on_treeView_doubleClicked(const QModelIndex &index);
    void on_Tree_ContextMenu(const QPoint &pos);

private:
    void SaveProjectInCache(const QString project_path);
    void onProjectOpened(const QString& projectDir);

    QMenuBar* m_menuBar = nullptr;
    QStatusBar* m_statusBar = nullptr;
    QWidget* m_mainWidget = nullptr;
    QHBoxLayout* m_mainLayout = nullptr;
    QSplitter* m_verticalSplitter = nullptr;
    QSplitter* m_mainSplitter = nullptr;

    FilesTabWidget* m_filesTabWidget = nullptr;
    FileTreeView* m_filesTreeView = nullptr;
    TerminalWidget* m_terminal = nullptr;

    BuildManager* m_buildManager = nullptr;
    QString m_projectDir;

public slots:
    void on_NewProject();
    void on_OpenProject();
    void on_SaveFile();
    void on_ClosingProject();
    void on_openSettings();
    void on_Toggle_Terminal(bool checked);

signals:
    void saveFileSignal();
    void CloseProject();
};

#endif // IDEWINDOW_H
