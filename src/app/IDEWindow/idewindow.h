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


private:

    // - - Main Widgets - -
    QMenuBar* m_menuBar;
    QStatusBar* m_statusBar;
    QWidget* m_mainWidget;
    QHBoxLayout* m_mainLayout;
    QSplitter* m_verticalSplitter;  // splitter (вверх вниз)
    QSplitter* m_mainSplitter; 

    // - - General Widgets - -
    FilesTabWidget* m_filesTabWidget;

    // - - Sidebar Widgets - -
    QWidget* m_leftSidebar;
    FileTreeView* m_filesTreeView;

    // - - Terminal Widget - -
    TerminalWidget* m_terminal;


public slots:

    /**
     * @brief Создать новый проект (QMenuBar->File->NewProject)
    */
    void on_NewProject();

    /**
     * @brief Открыть другой проект (QMenuBar->File->OpenProject)
    */
    void on_OpenProject();

    /**
     * @brief Сохранить файл (QMenuBar->File->SaveFile)
    */
    void on_SaveFile();

    /**
     * @brief Закрыть проект (QMenuBar->File->CloseProject)
    */
    void on_ClosingProject();

    /**
     * @brief Нажатие на Settings (QMenuBar->Edit->Settings)
     *
     * Открывает окно Settings
    */
    void on_openSettings();

    /**
     * @brief Отображение терминала
    */
    void on_Toggle_Terminal(bool checked);

    /**
     * @brief Отображение дерева файлов
    */
    void on_Toggle_FileTree(bool checked);


signals:
    void saveFileSignal();
    void CloseProject();

};
#endif // IDEWINDOW_H
