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

class IDEWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit IDEWindow(const QString &ProjectPath, QWidget *parent = nullptr);
    ~IDEWindow() override;

private:
    FileTab* currentFileTab() const;

    // - - Main Widgets - -
    QMenuBar* m_menuBar;
    QStatusBar* m_statusBar;
    QLabel* m_statusLabel;
    QWidget* m_mainWidget;
    QHBoxLayout* m_mainLayout;
    QSplitter* m_verticalSplitter;  // splitter (вверх вниз)
    QSplitter* m_mainSplitter;

    // - - General Widgets - -
    FilesTabWidget* m_filesTabWidget;

    // - - Sidebar Widgets - -
    QWidget* m_leftSidebar;
    FileTreePanel* m_filesTreeView;

    // - - Terminal Widget - -
    TerminalWidget *m_terminal;
    QString m_projectPath;

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
     * @brief Переключение переноса строк в редакторах кода
     */
    void on_SetWordWrap(bool checked);

    /**
     * @brief Переключение вставки пробелов вместо tab в редакторах кода
     */
    void on_SetTabReplace(bool checked);

    /**
     * @brief Изменение визуальной ширины tab в редакторах кода
     */
    void on_SetTabWidth(int width);

    /**
     * @brief Отображение дерева файлов
    */
    void on_Toggle_FileTree(bool checked) const;

signals:
    void saveFileSignal();
    void CloseProject();

    void setWordWrapSignal(bool checked);
    void setTabReplaceSignal(bool checked);
    void setTabWidthSignal(int width);

    void openTabModule(ModuleDescription<TabBase> desc);
};
#endif // IDEWINDOW_H
