#pragma once

#include "ui/MenuBar/basemenu.h"
#include "core/git/gitmanager.h"
#include <QMenu>
#include <QAction>
#include <QLineEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>

/**
 * @brief Git menu for repository operations
 *
 * Contains all Git operations: branches, commits, synchronization,
 * merge, staging, repository, and additional functions.
 */
class GitMenu : public BaseMenu
{
    Q_OBJECT

public:
    GitMenu();
    void setupConnections(IDEWindow* ideWind) override;

private:
    // Pointers to manager and IDE window
    GitManager *m_git = nullptr;
    IDEWindow *m_ideWind = nullptr;

    // Submenus
    QMenu *m_branchMenu;        // Branches
    QMenu *m_commitMenu;        // Commits
    QMenu *m_syncMenu;          // Synchronization
    QMenu *m_mergeMenu;         // Merge
    QMenu *m_stagingMenu;       // Staging
    QMenu *m_repoMenu;          // Repository
    QMenu *m_extraMenu;         // Additional

    // Branch actions
    QAction *m_checkoutBranch;
    QAction *m_createBranch;
    QAction *m_deleteBranch;
    QAction *m_renameBranch;
    QAction *m_listBranches;

    // Commit actions
    QAction *m_createCommit;
    QAction *m_showHistory;
    QAction *m_checkoutCommit;
    QAction *m_resetHard;
    QAction *m_resetMixed;
    QAction *m_revertCommit;
    QAction *m_amendCommit;

    // Synchronization actions
    QAction *m_push;
    QAction *m_pull;
    QAction *m_fetch;

    // Merge actions
    QAction *m_mergeBranch;
    QAction *m_showConflicts;

    // Staging actions
    QAction *m_stageFile;
    QAction *m_unstageFile;
    QAction *m_showDiff;
    QAction *m_showStagedDiff;

    // Repository actions
    QAction *m_cloneRepo;
    QAction *m_initRepo;
    QAction *m_openRepo;

    // Additional actions
    QAction *m_showStatus;
    QAction *m_stashSave;
    QAction *m_stashApply;
    QAction *m_stashDrop;
    QAction *m_stashList;
    QAction *m_showLogGraph;

    /** @brief Show error message */
    void showError(const QString &title, const QString &message);

    /** @brief Show information message */
    void showInfo(const QString &title, const QString &message);

    /** @brief Request text input */
    QString inputDialog(const QString &title, const QString &label);

    /** @brief Find git repository root (searches .git in all parent directories) */
    static QString findGitRepositoryRoot(const QString &path);

    /** @brief Check if path is a git repository */
    static bool isGitRepository(const QString &path);

    // Slots for actions
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

    /** @brief Timer for automatic repository monitoring */
    QTimer m_repoWatchTimer;
};
