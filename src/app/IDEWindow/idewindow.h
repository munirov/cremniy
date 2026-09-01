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
#include "project_info_manager.h"

class TerminalPanel;

class SearchPanel;
class QShortcut;

class IDEWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit IDEWindow(const QString &ProjectPath, QWidget *parent = nullptr);
    ~IDEWindow() override;
    bool gitBlameEnabled() const;

private:
    FileTab* currentFileTab() const;
    void showSearch(SearchScope scope, bool replaceMode);
    void configurateBuild();
    void openBuildConfigurate();

    // - - Project Info - -
    ProjectInfo m_projectInfo;

    // - - Main Widgets - -
    QMenuBar* m_menuBar;
    QStatusBar* m_statusBar;
    QLabel* m_statusLabel;
    QWidget* m_mainWidget;
    QHBoxLayout* m_mainLayout;
    QSplitter* m_verticalSplitter;  // splitter (вверх вниз)
    QSplitter* m_mainSplitter;

    // - - General Widgets - -
    FilesTabWidget* m_filesTabWidget = nullptr;

    // - - Sidebar Widgets - -
    QWidget* m_leftSidebar;
    FileTreePanel* m_filesTreeView;

    // - - Terminal Widget - -
    TerminalPanel *m_terminalPanel;
    SearchPanel* m_searchPanel;
    QShortcut* m_closeSearchShortcut;
    QString m_projectPath;

public slots:

    /**
     * @brief Собрать текущий проект (QMenuBar->Build->Build)
    */
    void on_Build();

    /**
     * @brief Открыть конфигурацию сборки (QMenuBar->Build->Configurate)
    */
    void on_openBuildConfigurate();

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
    void on_Find();
    void on_FindOpenFiles();
    void on_FindInProject();
    void on_Replace();

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

    /** @brief Route the Git blame command to modules through TabBase. */
    void on_SetGitBlame(bool enabled);

    /**
     * @brief Отображение дерева файлов
    */
    void on_Toggle_FileTree(bool checked) const;

signals:
    void saveFileSignal();
    void CloseProject();
    void terminalVisibilityChanged(bool visible);

    void setWordWrapSignal(bool checked);
    void setTabReplaceSignal(bool checked);
    void setTabWidthSignal(int width);
    void setGitBlameSignal(bool enabled);
    void gitBlameEnabledChanged(bool enabled);

    void openTabModule(ModuleDescription<TabBase> desc);
};
#endif // IDEWINDOW_H
