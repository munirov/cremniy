#ifndef IDEWINDOW_H
#define IDEWINDOW_H

#include "filestabwidget.h"
#include "filetreeview.h"
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

    /**
     * @brief Двойной клик
     *
     * Обрабатывает открытие файла или разворачивание директории
    */
    void on_treeView_doubleClicked(const QModelIndex &index);

    /**
     * @brief Открытие контекстного меню
     *
     * Нужен при клике на ПКМ для открытия контекстного меню
    */
    void on_Tree_ContextMenu(const QPoint &pos);

    /**
     * @brief Закрыть проект (QMenuBar->File->CloseProject)
    */
    void on_ClosingProject();

    /**
     * @brief Нажатие на Settings (QMenuBar->Edit->Settings)
     *
     * Открывает окно Settings
    */
    void on_Open_Settings();

    /**
     * @brief Нажатие на Reverse Calculator (QMenuBar->Tools->ReverseCalculator)
     *
     * Открывает калькулятор для перевода чисел в разные систмы счислений
    */
    void on_Open_ReverseCalculator();

private:

    /**
     * @brief Сохранить текущий путь проекта в истории
    */
    void SaveProjectInCache(const QString& project_path);

    // - - Menu Bars - -
    QMenu* m_fileMenu;
    QMenu* m_editMenu;
    QMenu* m_viewMenu;
    QMenu* m_toolsMenu;
    QMenu* m_referencesMenu;
    QMenu* m_gitMenu;

    // - - File Menu - -
    QAction* m_file_newProject;
    QAction* m_file_openProject;
    QAction* m_file_saveFile;
    QAction* m_file_closeProject;

    // - - Edit Menu - -
    QAction* m_edit_settings;

    // - - View Menu - -
    QAction* m_view_wordWrap;

    // - - Tools Menu - -

    QAction* m_tools_reverseCalculator;

    // - - References - -
    QAction* m_references_asciiChars;
    QAction* m_references_keybScancodes;

    // - - Git Menu - -
    QAction* m_git_commit;
    QAction* m_git_commitAndPush;
    QAction* m_git_setBranch;

    // - - Main Widgets - -
    QMenuBar* m_menuBar;
    QStatusBar* m_statusBar;
    QWidget* m_mainWidget;
    QHBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // - - General Widgets - -
    FilesTabWidget* m_filesTabWidget;
    FileTreeView* m_filesTreeView;

};
#endif // IDEWINDOW_H
