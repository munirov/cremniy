#ifndef IDEWINDOW_H
#define IDEWINDOW_H

#include "core/modules/ModuleManager.h"
#include "ui/FilesTabWidget/filestabwidget.h"
#include "widgets/filetreepanel.h"
#include "core/settings/exclusionfilterproxymodel.h"
#include <QMainWindow>
#include <qboxlayout.h>
#include <qsplitter.h>
#include <qstatusbar.h>
#include <QLabel>
#include "widgets/terminal/terminalwidget.h"

class SearchPanelWidget;

class IDEWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit IDEWindow(const QString &ProjectPath, QWidget *parent = nullptr);
    ~IDEWindow() override;

    QString projectPath() const { return m_projectPath; }

private:
    FileTab* currentFileTab() const;

    // - - Main Widgets - -
    QMenuBar* m_menuBar;
    QStatusBar* m_statusBar;
    QLabel* m_statusLabel;
    QWidget* m_mainWidget;
    QHBoxLayout* m_mainLayout;
    QSplitter* m_verticalSplitter;
    QSplitter* m_mainSplitter;

    // - - General Widgets - -
    FilesTabWidget* m_filesTabWidget;

    // - - Sidebar Widgets - -
    QWidget* m_leftSidebar;
    FileTreePanel* m_filesTreeView;

    // - - Right Sidebar (Search) - -
    QWidget* m_rightSidebar = nullptr;
    SearchPanelWidget* m_searchPanel = nullptr;
    int m_searchMode = -1; // -1=none, 0=in-file, 1=project

    // - - Terminal Widget - -
    TerminalWidget *m_terminal;
    QString m_projectPath;

    void updateSearchEditor();

public slots:
    void on_NewProject();
    void on_OpenProject();
    void on_SaveFile();
    void on_ClosingProject();
    void on_openSettings();
    void on_Toggle_Terminal(bool checked);
    void on_SetWordWrap(bool checked);
    void on_SetTabReplace(bool checked);
    void on_SetTabWidth(int width);
    void on_Toggle_FileTree(bool checked) const;
    void on_Toggle_Search(bool checked);
    void on_FindInFile();
    void on_FindInProject();

signals:
    void saveFileSignal();
    void CloseProject();

    void setWordWrapSignal(bool checked);
    void setTabReplaceSignal(bool checked);
    void setTabWidthSignal(int width);

    void openTabModule(ModuleDescription<TabBase> desc);
};
#endif // IDEWINDOW_H
