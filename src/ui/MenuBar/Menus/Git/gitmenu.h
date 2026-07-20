#ifndef GITMENU_H
#define GITMENU_H

#include "ui/MenuBar/basemenu.h"
#include "core/git/gitmanager.h"
#include <QMenu>
#include <QAction>
#include <QLineEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>

/**
 * @brief Меню Git для работы с репозиторием
 *
 * Содержит все операции с Git: ветки, коммиты, синхронизация,
 * слияние, индексация, репозиторий и дополнительные функции.
 */
class GitMenu : public BaseMenu
{
    Q_OBJECT

public:
    GitMenu();
    void setupConnections(IDEWindow* ideWind) override;

private:
    // Указатели на менеджер и IDE окно
    GitManager *m_git = nullptr;
    IDEWindow *m_ideWind = nullptr;

    // Подменю
    QMenu *m_branchMenu;        // Ветки
    QMenu *m_commitMenu;        // Коммиты
    QMenu *m_syncMenu;          // Синхронизация
    QMenu *m_mergeMenu;         // Слияние
    QMenu *m_stagingMenu;       // Индексация
    QMenu *m_repoMenu;          // Репозиторий
    QMenu *m_extraMenu;         // Дополнительно

    // Действия веток
    QAction *m_checkoutBranch;
    QAction *m_createBranch;
    QAction *m_deleteBranch;
    QAction *m_renameBranch;
    QAction *m_listBranches;

    // Действия коммитов
    QAction *m_createCommit;
    QAction *m_showHistory;
    QAction *m_checkoutCommit;
    QAction *m_resetHard;
    QAction *m_resetMixed;
    QAction *m_revertCommit;
    QAction *m_amendCommit;

    // Действия синхронизации
    QAction *m_push;
    QAction *m_pull;
    QAction *m_fetch;

    // Действия слияния
    QAction *m_mergeBranch;
    QAction *m_showConflicts;

    // Действия индексации
    QAction *m_stageFile;
    QAction *m_unstageFile;
    QAction *m_showDiff;
    QAction *m_showStagedDiff;

    // Действия репозитория
    QAction *m_cloneRepo;
    QAction *m_initRepo;
    QAction *m_openRepo;

    // Дополнительные действия
    QAction *m_showStatus;
    QAction *m_stashSave;
    QAction *m_stashApply;
    QAction *m_stashDrop;
    QAction *m_stashList;
    QAction *m_showLogGraph;

    /** @brief Показать сообщение об ошибке */
    void showError(const QString &title, const QString &message);

    /** @brief Показать информационное сообщение */
    void showInfo(const QString &title, const QString &message);

    /** @brief Запросить ввод текста */
    QString inputDialog(const QString &title, const QString &label);

    /** @brief Найти корень git-репозитория (ищет .git во всех родительских директориях) */
    static QString findGitRepositoryRoot(const QString &path);

    /** @brief Проверить, является ли путь гит репозиторием */
    static bool isGitRepository(const QString &path);

    // Слоты для действий
    void onCheckoutBranch();
    void onCreateBranch();
    void onDeleteBranch();
    void onRenameBranch();
    void onListBranches();

    void onCreateCommit();
    void onShowHistory();
    void onCheckoutCommit();
    void onResetHard();
    void onResetMixed();
    void onRevertCommit();
    void onAmendCommit();

    void onPush();
    void onPull();
    void onFetch();

    void onMergeBranch();
    void onShowConflicts();

    void onStageFile();
    void onUnstageFile();
    void onShowDiff();
    void onShowStagedDiff();

    void onCloneRepo();
    void onInitRepo();
    void onOpenRepo();

    void onShowStatus();
    void onStashSave();
    void onStashApply();
    void onStashDrop();
    void onStashList();
    void onShowLogGraph();

    void onRepoWatchTimeout();

    /** @brief Таймер автоматического отслеживания репозитория */
    QTimer m_repoWatchTimer;
};

#endif // GITMENU_H