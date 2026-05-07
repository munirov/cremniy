#ifndef IDEWINDOW_H
#define IDEWINDOW_H

#include "filestabwidget.h"
#include "filetreeview.h"
#include <QMainWindow>
#include <qboxlayout.h>
#include <qmenubar.h>
#include <qsplitter.h>
#include <qstatusbar.h>
#include "widgets/terminal/terminalwidget.h"

class IDEWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit IDEWindow(QString ProjectPath, QWidget *parent = nullptr);
    ~IDEWindow() override;

private slots:

    /**
     * @brief Double click
     *
     * Handles file opening or directory expansion
    */
    void on_treeView_doubleClicked(const QModelIndex &index);

    /**
     * @brief Open context menu
     *
     * Required for context menu on right click (RMB)
    */
    void on_Tree_ContextMenu(const QPoint &pos);


private:

    /* - - Main Widgets - - */
    QMenuBar* m_menuBar;
    QStatusBar* m_statusBar;
    QWidget* m_mainWidget;
    QHBoxLayout* m_mainLayout;
    QSplitter* m_verticalSplitter;  // splitter (вверх вниз)
    QSplitter* m_mainSplitter; 

    /* - - General Widgets - - */
    FilesTabWidget* m_filesTabWidget;
    FileTreeView* m_filesTreeView;

    /* - - Terminal Widget - - */
    TerminalWidget* m_terminal;


public slots:

    /**
     * @brief Create new project (QMenuBar->File->NewProject)
    */
    void on_NewProject();

    /**
     * @brief Open another project (QMenuBar->File->OpenProject)
    */
    void on_OpenProject();

    /**
     * @brief Save file (QMenuBar->File->SaveFile)
    */
    void on_SaveFile();

    /**
     * @brief Close project (QMenuBar->File->CloseProject)
    */
    void on_ClosingProject();

    /**
     * @brief Triggered on Settings (QMenuBar->Edit->Settings)
     *
     * Opens the settings window
    */
    void on_openSettings();

    /**
     * @brief Terminal Display
    */
    void on_Toggle_Terminal(bool checked);


signals:
    void saveFileSignal();
    void CloseProject();

};
#endif // IDEWINDOW_H
