#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <git2.h>

/**
 * @brief Wrapper class over libgit2 for all git operations
 * All methods return true on success and false on error.
 * Error text can be obtained via lastError().
 */
class GitManager : public QObject
{
    Q_OBJECT

public:
    explicit GitManager(QObject *parent = nullptr);
    ~GitManager() override;

    /** @brief Open repository at path */
    bool open(const QString &repoPath);

    /** @brief Close current repository */
    void close();

    /** @brief Check if repository is open */
    bool isOpen() const;

    /** @brief Get last error */
    QString lastError() const;

    /** @brief Get repository path */
    QString repoPath() const;

    /* Branches */

    /** @brief Get list of all branches */
    QStringList branches() const;

    /** @brief Get current branch */
    QString currentBranch() const;

    /** @brief Checkout branch */
    bool checkoutBranch(const QString &branchName);

    /** @brief Create new branch */
    bool createBranch(const QString &branchName);

    /** @brief Delete branch */
    bool deleteBranch(const QString &branchName);

    /** @brief Rename branch */
    bool renameBranch(const QString &oldName, const QString &newName);

    /* Commits */

    /** @brief Create commit */
    bool createCommit(const QString &message);

    /** @brief Get commit history (returns OIDs as hex strings) */
    QStringList commitHistory(int count = 50) const;

    /** @brief Get commit message by OID */
    QString commitMessage(const QString &oid) const;

    /** @brief Get commit author by OID */
    QString commitAuthor(const QString &oid) const;

    /** @brief Checkout commit (detached HEAD) */
    bool checkoutCommit(const QString &oid);

    /** @brief Reset commit (reset --hard) */
    bool resetHard(const QString &oid);

    /** @brief Reset commit (reset --mixed) */
    bool resetMixed(const QString &oid);

    /** @brief Revert commit */
    bool revertCommit(const QString &oid);

    /** @brief Amend last commit */
    bool amendCommit(const QString &message);

    /* Synchronization */

    /** @brief Push changes */
    bool push(const QString &remote = "origin", const QString &branch = "");

    /** @brief Pull changes */
    bool pull(const QString &remote = "origin", const QString &branch = "");

    /** @brief Fetch changes */
    bool fetch(const QString &remote = "origin");

    /* Merge */

    /** @brief Merge branch */
    bool merge(const QString &branchName);

    /** @brief Check if there are conflicts */
    bool hasConflicts() const;

    /** @brief Get list of conflict files */
    QStringList conflictFiles() const;

    /* Staging */

    /** @brief Stage file */
    bool stageFile(const QString &filePath);

    /** @brief Unstage file */
    bool unstageFile(const QString &filePath);

    /** @brief Get diff for file */
    QString fileDiff(const QString &filePath) const;

    /** @brief Get diff for staged changes */
    QString stagedDiff() const;

    /* Repository */

    /** @brief Clone repository */
    bool clone(const QString &url, const QString &path);

    /** @brief Initialize repository */
    bool init(const QString &path);

    /* Additional */

    /** @brief Get repository status */
    QString status() const;

    /** @brief Save stash */
    bool stashSave(const QString &message = "");

    /** @brief Apply stash */
    bool stashApply(int index = 0);

    /** @brief Drop stash */
    bool stashDrop(int index = 0);

    /** @brief Get stash list */
    QStringList stashList() const;

    /** @brief Get log with branch graph (text representation) */
    QString logGraph(int count = 50) const;

signals:
    /** @brief Signal on repository change */
    void repositoryChanged();

private:
    git_repository *m_repo = nullptr;
    QString m_repoPath;
    mutable QString m_lastError;

    /** @brief Set error message */
    void setError(const QString &error) const;

    /** @brief Get username from config */
    QString userName() const;

    /** @brief Get user email from config */
    QString userEmail() const;

    /** @brief Create signature */
    git_signature *createSignature() const;
};
