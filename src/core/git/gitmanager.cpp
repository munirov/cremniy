#include "gitmanager.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>

GitManager::GitManager(QObject *parent)
    : QObject(parent)
    , m_repo(nullptr)
{
    // Инициализируем один раз
    git_libgit2_init();
}

GitManager::~GitManager()
{
    close();
    git_libgit2_shutdown();
}

bool GitManager::open(const QString &repoPath)
{
    close();

    int error = git_repository_open(&m_repo, repoPath.toUtf8().constData());
    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Failed to open repository"));
        return false;
    }

    m_repoPath = repoPath;
    return true;
}

void GitManager::close()
{
    if (m_repo) {
        git_repository_free(m_repo);
        m_repo = nullptr;
    }
    m_repoPath.clear();
}

bool GitManager::isOpen() const
{
    return m_repo != nullptr;
}

QString GitManager::lastError() const
{
    return m_lastError;
}

QString GitManager::repoPath() const
{
    return m_repoPath;
}

void GitManager::setError(const QString &error) const
{
    m_lastError = error;
}

// Ветки

QStringList GitManager::branches() const
{
    QStringList result;
    if (!m_repo) return result;

    git_branch_iterator *iter = nullptr;
    int error = git_branch_iterator_new(&iter, m_repo, GIT_BRANCH_LOCAL);
    if (error != 0) return result;

    git_reference *ref = nullptr;
    git_branch_t type;
    while (git_branch_next(&ref, &type, iter) == 0) {
        const char *name = nullptr;
        if (git_branch_name(&name, ref) == 0) {
            result.append(QString::fromUtf8(name));
        }
        git_reference_free(ref);
    }
    git_branch_iterator_free(iter);

    return result;
}

QString GitManager::currentBranch() const
{
    if (!m_repo) return {};

    git_reference *head = nullptr;
    int error = git_repository_head(&head, m_repo);
    if (error != 0) return {};

    const char *name = nullptr;
    git_branch_name(&name, head);
    QString branchName = QString::fromUtf8(name);
    git_reference_free(head);

    return branchName;
}

bool GitManager::checkoutBranch(const QString &branchName)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    // ищем ветку
    git_reference *ref = nullptr;
    QString refName = "refs/heads/" + branchName;
    int error = git_reference_dwim(&ref, m_repo, refName.toUtf8().constData());
    if (error != 0) {
        setError(tr("Branch not found: ") + branchName);
        return false;
    }

    // безопасный чекаут
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    error = git_checkout_tree(m_repo, (const git_object *)git_reference_target(ref), &opts);
    if (error != 0) {
        git_reference_free(ref);
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Checkout error"));
        return false;
    }

    error = git_repository_set_head(m_repo, refName.toUtf8().constData());
    git_reference_free(ref);

    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("HEAD setup error"));
        return false;
    }

    emit repositoryChanged();
    return true;
}

bool GitManager::createBranch(const QString &branchName)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    // берём  коммит
    git_oid head_oid;
    int error = git_reference_name_to_id(&head_oid, m_repo, "HEAD");
    if (error != 0) {
        setError(tr("Failed to get HEAD"));
        return false;
    }

    git_commit *commit = nullptr;
    error = git_commit_lookup(&commit, m_repo, &head_oid);
    if (error != 0) {
        setError(tr("Failed to find HEAD commit"));
        return false;
    }

    git_reference *new_ref = nullptr;
    error = git_branch_create(&new_ref, m_repo, branchName.toUtf8().constData(), commit, 0);
    git_commit_free(commit);

    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Failed to create branch"));
        return false;
    }

    git_reference_free(new_ref);
    emit repositoryChanged();
    return true;
}

bool GitManager::deleteBranch(const QString &branchName)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    git_reference *ref = nullptr;
    QString refName = "refs/heads/" + branchName;
    int error = git_reference_dwim(&ref, m_repo, refName.toUtf8().constData());
    if (error != 0) {
        setError(tr("Branch not found: ") + branchName);
        return false;
    }

    error = git_branch_delete(ref);
    git_reference_free(ref);

    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Failed to delete branch"));
        return false;
    }

    emit repositoryChanged();
    return true;
}

bool GitManager::renameBranch(const QString &oldName, const QString &newName)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    git_reference *ref = nullptr;
    QString refName = "refs/heads/" + oldName;
    int error = git_reference_dwim(&ref, m_repo, refName.toUtf8().constData());
    if (error != 0) {
        setError(tr("Branch not found: ") + oldName);
        return false;
    }

    git_reference *new_ref = nullptr;
    error = git_branch_move(&new_ref, ref, newName.toUtf8().constData(), 0);
    git_reference_free(ref);

    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Failed to rename branch"));
        return false;
    }

    git_reference_free(new_ref);
    emit repositoryChanged();
    return true;
}

// Коммиты

bool GitManager::createCommit(const QString &message)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    // получаем индекс
    git_index *index = nullptr;
    int error = git_repository_index(&index, m_repo);
    if (error != 0) {
        setError(tr("Failed to get index"));
        return false;
    }

    // создаём tree из индекса
    git_oid tree_oid;
    error = git_index_write_tree(&tree_oid, index);
    if (error != 0) {
        git_index_free(index);
        setError(tr("Failed to write tree"));
        return false;
    }

    git_tree *tree = nullptr;
    error = git_tree_lookup(&tree, m_repo, &tree_oid);
    git_index_free(index);
    if (error != 0) {
        setError(tr("Failed to find tree"));
        return false;
    }

    // получаем коммит как родителя
    git_commit *parent = nullptr;
    git_oid head_oid;
    error = git_reference_name_to_id(&head_oid, m_repo, "HEAD");
    if (error == 0) {
        git_commit_lookup(&parent, m_repo, &head_oid);
    }

    // создаём подпись
    git_signature *sig = createSignature();
    if (!sig) {
        git_tree_free(tree);
        if (parent) git_commit_free(parent);
        return false;
    }

    // создаём коммит
    git_oid commit_oid;
    if (parent) {
        error = git_commit_create(
            &commit_oid, m_repo, "HEAD",
            sig, sig, nullptr,
            message.toUtf8().constData(),
            tree, 1, (const git_commit **)&parent);
    } else {
        error = git_commit_create(
            &commit_oid, m_repo, "HEAD",
            sig, sig, nullptr,
            message.toUtf8().constData(),
            tree, 0, nullptr);
    }

    git_signature_free(sig);
    git_tree_free(tree);
    if (parent) git_commit_free(parent);

    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Failed to create commit"));
        return false;
    }

    emit repositoryChanged();
    return true;
}

QStringList GitManager::commitHistory(int count) const
{
    QStringList result;
    if (!m_repo) return result;

    git_revwalk *walker = nullptr;
    int error = git_revwalk_new(&walker, m_repo);
    if (error != 0) return result;

    git_revwalk_sorting(walker, GIT_SORT_TIME);
    git_revwalk_push_head(walker);

    git_oid oid;
    int i = 0;
    while (git_revwalk_next(&oid, walker) == 0 && i < count) {
        result.append(QString::fromUtf8(git_oid_tostr_s(&oid)));
        i++;
    }

    git_revwalk_free(walker);
    return result;
}

QString GitManager::commitMessage(const QString &oid) const
{
    if (!m_repo) return {};

    git_oid commit_oid;
    if (git_oid_fromstr(&commit_oid, oid.toUtf8().constData()) != 0)
        return {};

    git_commit *commit = nullptr;
    if (git_commit_lookup(&commit, m_repo, &commit_oid) != 0)
        return {};

    QString msg = QString::fromUtf8(git_commit_message(commit));
    git_commit_free(commit);
    return msg.trimmed();
}

QString GitManager::commitAuthor(const QString &oid) const
{
    if (!m_repo) return {};

    git_oid commit_oid;
    if (git_oid_fromstr(&commit_oid, oid.toUtf8().constData()) != 0)
        return {};

    git_commit *commit = nullptr;
    if (git_commit_lookup(&commit, m_repo, &commit_oid) != 0)
        return {};

    const git_signature *sig = git_commit_author(commit);
    QString author = QString::fromUtf8(sig->name) + " <" + QString::fromUtf8(sig->email) + ">";
    git_commit_free(commit);
    return author;
}

bool GitManager::checkoutCommit(const QString &oid)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    git_oid commit_oid;
    if (git_oid_fromstr(&commit_oid, oid.toUtf8().constData()) != 0) {
        setError(tr("Invalid commit OID"));
        return false;
    }

    git_commit *commit = nullptr;
    int error = git_commit_lookup(&commit, m_repo, &commit_oid);
    if (error != 0) {
        setError(tr("Commit not found"));
        return false;
    }

    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    error = git_checkout_tree(m_repo, (const git_object *)commit, &opts);
    if (error != 0) {
        git_commit_free(commit);
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Checkout error"));
        return false;
    }

    git_repository_set_head_detached(m_repo, &commit_oid);
    git_commit_free(commit);

    emit repositoryChanged();
    return true;
}

bool GitManager::resetHard(const QString &oid)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    git_oid commit_oid;
    if (git_oid_fromstr(&commit_oid, oid.toUtf8().constData()) != 0) {
        setError(tr("Invalid commit OID"));
        return false;
    }

    git_object *target = nullptr;
    int error = git_object_lookup(&target, m_repo, &commit_oid, GIT_OBJECT_COMMIT);
    if (error != 0) {
        setError(tr("Object not found"));
        return false;
    }

    error = git_reset(m_repo, target, GIT_RESET_HARD, nullptr);
    git_object_free(target);

    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Reset error"));
        return false;
    }

    emit repositoryChanged();
    return true;
}

bool GitManager::resetMixed(const QString &oid)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    git_oid commit_oid;
    if (git_oid_fromstr(&commit_oid, oid.toUtf8().constData()) != 0) {
        setError(tr("Invalid commit OID"));
        return false;
    }

    git_object *target = nullptr;
    int error = git_object_lookup(&target, m_repo, &commit_oid, GIT_OBJECT_COMMIT);
    if (error != 0) {
        setError(tr("Object not found"));
        return false;
    }

    error = git_reset(m_repo, target, GIT_RESET_MIXED, nullptr);
    git_object_free(target);

    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Reset error"));
        return false;
    }

    emit repositoryChanged();
    return true;
}

bool GitManager::revertCommit(const QString &oid)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    git_oid commit_oid;
    if (git_oid_fromstr(&commit_oid, oid.toUtf8().constData()) != 0) {
        setError(tr("Invalid commit OID"));
        return false;
    }

    git_commit *commit = nullptr;
    int error = git_commit_lookup(&commit, m_repo, &commit_oid);
    if (error != 0) {
        setError(tr("Commit not found"));
        return false;
    }

    error = git_revert(m_repo, commit, nullptr);
    git_commit_free(commit);

    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Revert error"));
        return false;
    }

    emit repositoryChanged();
    return true;
}

bool GitManager::amendCommit(const QString &message)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    // берём коммит
    git_oid head_oid;
    int error = git_reference_name_to_id(&head_oid, m_repo, "HEAD");
    if (error != 0) {
        setError(tr("Failed to get HEAD"));
        return false;
    }

    git_commit *old_commit = nullptr;
    error = git_commit_lookup(&old_commit, m_repo, &head_oid);
    if (error != 0) {
        setError(tr("Failed to find HEAD commit"));
        return false;
    }

    // создаём подпись
    git_signature *sig = createSignature();
    if (!sig) {
        git_commit_free(old_commit);
        return false;
    }

    // берём tree из старого коммита
    git_tree *tree = nullptr;
    error = git_commit_tree(&tree, old_commit);
    if (error != 0) {
        git_signature_free(sig);
        git_commit_free(old_commit);
        setError(tr("Failed to get commit tree"));
        return false;
    }

    // создаём новый коммит с тем же деревом, но новым сообщением
    git_oid new_commit_oid;
    unsigned int parent_count = git_commit_parentcount(old_commit);
    QVector<const git_commit *> old_parents;
    for (unsigned int i = 0; i < parent_count; i++) {
        git_commit *parent = nullptr;
        git_commit_parent(&parent, old_commit, i);
        old_parents.append(parent);
    }

    error = git_commit_create(
        &new_commit_oid, m_repo, "HEAD",
        sig, sig, nullptr,
        message.toUtf8().constData(),
        tree,
        parent_count,
        parent_count > 0 ? old_parents.data() : nullptr);

    git_signature_free(sig);
    git_tree_free(tree);
    git_commit_free(old_commit);

    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Amend error"));
        return false;
    }

    emit repositoryChanged();
    return true;
}

// Синхронизация

bool GitManager::push(const QString &remote, const QString &branch)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    // ищем remote
    git_remote *rem = nullptr;
    int error = git_remote_lookup(&rem, m_repo, remote.toUtf8().constData());
    if (error != 0) {
        setError(tr("Remote not found: ") + remote);
        return false;
    }

    // ветка для пуша
    QString branchRef = branch.isEmpty() ? currentBranch() : branch;
    if (branchRef.isEmpty()) {
        git_remote_free(rem);
        setError(tr("Push branch not specified"));
        return false;
    }

    QString refspec = "refs/heads/" + branchRef + ":refs/heads/" + branchRef;

    git_strarray refspecs;
    refspecs.count = 1;
    refspecs.strings = new char*[1];
    QByteArray refspecBytes = refspec.toUtf8();
    refspecs.strings[0] = refspecBytes.data();

    git_push_options push_opts = GIT_PUSH_OPTIONS_INIT;

    error = git_remote_push(rem, &refspecs, &push_opts);
    delete[] refspecs.strings;
    git_remote_free(rem);

    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Push error"));
        return false;
    }

    return true;
}

bool GitManager::pull(const QString &remote, const QString &branch)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    // сначала получаем изменения
    if (!fetch(remote)) return false;

    // потом сливаем
    QString branchRef = branch.isEmpty() ? currentBranch() : branch;
    if (branchRef.isEmpty()) {
        setError(tr("Branch not specified"));
        return false;
    }

    QString remoteRef = "refs/remotes/" + remote + "/" + branchRef;
    return merge(remoteRef);
}

bool GitManager::fetch(const QString &remote)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    git_remote *rem = nullptr;
    int error = git_remote_lookup(&rem, m_repo, remote.toUtf8().constData());
    if (error != 0) {
        setError(tr("Remote not found: ") + remote);
        return false;
    }

    git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
    error = git_remote_fetch(rem, nullptr, &fetch_opts, nullptr);
    git_remote_free(rem);

    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Fetch error"));
        return false;
    }

    return true;
}

// Слияние

bool GitManager::merge(const QString &branchName)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    // ищем коммит для слияния
    git_oid merge_oid;
    QString refName = branchName.startsWith("refs/") ? branchName : "refs/heads/" + branchName;
    int error = git_reference_name_to_id(&merge_oid, m_repo, refName.toUtf8().constData());
    if (error != 0) {
        setError(tr("Failed to find: ") + branchName);
        return false;
    }

    git_annotated_commit *their_heads[1] = { nullptr };
    error = git_annotated_commit_lookup(&their_heads[0], m_repo, &merge_oid);
    if (error != 0) {
        setError(tr("Failed to find commit for merge"));
        return false;
    }

    git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    error = git_merge(m_repo, (const git_annotated_commit **)their_heads, 1, &merge_opts, &checkout_opts);
    git_annotated_commit_free(their_heads[0]);

    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Merge error"));
        return false;
    }

    // если нет конфликтов, делаем коммит слияния
    if (!hasConflicts()) {
        git_index *index = nullptr;
        git_repository_index(&index, m_repo);

        git_oid tree_oid;
        git_index_write_tree(&tree_oid, index);
        git_index_free(index);

        git_tree *tree = nullptr;
        git_tree_lookup(&tree, m_repo, &tree_oid);

        git_oid head_oid;
        git_reference_name_to_id(&head_oid, m_repo, "HEAD");
        git_commit *head_commit = nullptr;
        git_commit_lookup(&head_commit, m_repo, &head_oid);

        git_commit *other_commit = nullptr;
        git_commit_lookup(&other_commit, m_repo, &merge_oid);

        git_signature *sig = createSignature();
        if (sig) {
            const git_commit *parents[] = { head_commit, other_commit };
            git_oid commit_oid;
            git_commit_create(
                &commit_oid, m_repo, "HEAD",
                sig, sig, nullptr,
                (tr("Merge branch '") + branchName + "'").toUtf8().constData(),
                tree, 2, parents);
            git_signature_free(sig);
        }

        git_tree_free(tree);
        git_commit_free(head_commit);
        git_commit_free(other_commit);
    }

    emit repositoryChanged();
    return true;
}

bool GitManager::hasConflicts() const
{
    if (!m_repo) return false;

    git_index *index = nullptr;
    if (git_repository_index(&index, m_repo) != 0) return false;

    bool hasConflict = git_index_has_conflicts(index);
    git_index_free(index);
    return hasConflict;
}

QStringList GitManager::conflictFiles() const
{
    QStringList result;
    if (!m_repo) return result;

    git_index *index = nullptr;
    if (git_repository_index(&index, m_repo) != 0) return result;

    git_index_conflict_iterator *iter = nullptr;
    if (git_index_conflict_iterator_new(&iter, index) != 0) {
        git_index_free(index);
        return result;
    }

    const git_index_entry *ancestor = nullptr;
    const git_index_entry *ours = nullptr;
    const git_index_entry *theirs = nullptr;

    while (git_index_conflict_next(&ancestor, &ours, &theirs, iter) == 0) {
        if (ours) {
            result.append(QString::fromUtf8(ours->path));
        } else if (theirs) {
            result.append(QString::fromUtf8(theirs->path));
        }
    }

    git_index_conflict_iterator_free(iter);
    git_index_free(index);
    return result;
}

// Индексация

bool GitManager::stageFile(const QString &filePath)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    git_index *index = nullptr;
    int error = git_repository_index(&index, m_repo);
    if (error != 0) {
        setError(tr("Failed to get index"));
        return false;
    }

    error = git_index_add_bypath(index, filePath.toUtf8().constData());
    if (error != 0) {
        git_index_free(index);
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Failed to add file"));
        return false;
    }

    error = git_index_write(index);
    git_index_free(index);

    if (error != 0) {
        setError(tr("Failed to write index"));
        return false;
    }

    emit repositoryChanged();
    return true;
}

bool GitManager::unstageFile(const QString &filePath)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    git_index *index = nullptr;
    int error = git_repository_index(&index, m_repo);
    if (error != 0) {
        setError(tr("Failed to get index"));
        return false;
    }

    error = git_index_remove_bypath(index, filePath.toUtf8().constData());
    if (error != 0) {
        git_index_free(index);
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Failed to remove file from index"));
        return false;
    }

    error = git_index_write(index);
    git_index_free(index);

    if (error != 0) {
        setError(tr("Failed to write index"));
        return false;
    }

    emit repositoryChanged();
    return true;
}

QString GitManager::fileDiff(const QString &filePath) const
{
    if (!m_repo) return {};

    git_diff *diff = nullptr;
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
    opts.flags = GIT_DIFF_INCLUDE_UNTRACKED;

    int error = git_diff_index_to_workdir(&diff, m_repo, nullptr, &opts);
    if (error != 0) return {};

    git_buf buf = {0};
    error = git_diff_to_buf(&buf, diff, GIT_DIFF_FORMAT_PATCH);
    git_diff_free(diff);

    if (error != 0) {
        git_buf_dispose(&buf);
        return {};
    }

    QString result = QString::fromUtf8(buf.ptr, buf.size);
    git_buf_dispose(&buf);
    return result;
}

QString GitManager::stagedDiff() const
{
    if (!m_repo) return {};

    git_diff *diff = nullptr;
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;

    int error = git_diff_tree_to_index(&diff, m_repo, nullptr, nullptr, &opts);
    if (error != 0) return {};

    git_buf buf = {0};
    error = git_diff_to_buf(&buf, diff, GIT_DIFF_FORMAT_PATCH);
    git_diff_free(diff);

    if (error != 0) {
        git_buf_dispose(&buf);
        return {};
    }

    QString result = QString::fromUtf8(buf.ptr, buf.size);
    git_buf_dispose(&buf);
    return result;
}

// Репозиторий

bool GitManager::clone(const QString &url, const QString &path)
{
    git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
    git_repository *cloned_repo = nullptr;

    int error = git_clone(&cloned_repo, url.toUtf8().constData(), path.toUtf8().constData(), &opts);
    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Clone error"));
        return false;
    }

    close();
    m_repo = cloned_repo;
    m_repoPath = path;

    emit repositoryChanged();
    return true;
}

bool GitManager::init(const QString &path)
{
    git_repository *new_repo = nullptr;
    int error = git_repository_init(&new_repo, path.toUtf8().constData(), 0);
    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Init error"));
        return false;
    }

    close();
    m_repo = new_repo;
    m_repoPath = path;

    emit repositoryChanged();
    return true;
}

// Дополнительно

QString GitManager::status() const
{
    if (!m_repo) return {};

    git_status_options opts = GIT_STATUS_OPTIONS_INIT;
    opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
                 GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
                 GIT_STATUS_OPT_SORT_CASE_SENSITIVELY;

    git_status_list *status_list = nullptr;
    int error = git_status_list_new(&status_list, m_repo, &opts);
    if (error != 0) return {};

    size_t count = git_status_list_entrycount(status_list);
    QString result;
    QString branch = currentBranch();

    result += tr("On branch ") + (branch.isEmpty() ? "HEAD" : branch) + "\n";

    for (size_t i = 0; i < count; i++) {
        const git_status_entry *entry = git_status_byindex(status_list, i);
        if (!entry) continue;

        QString path;
        if (entry->head_to_index) {
            path = QString::fromUtf8(entry->head_to_index->old_file.path);
        } else if (entry->index_to_workdir) {
            path = QString::fromUtf8(entry->index_to_workdir->old_file.path);
        }

        unsigned int flags = entry->status;

        if (flags & GIT_STATUS_INDEX_NEW)        result += "  " + tr("new:") + "    " + path + "\n";
        if (flags & GIT_STATUS_INDEX_MODIFIED)   result += "  " + tr("modified:") + " " + path + "\n";
        if (flags & GIT_STATUS_INDEX_DELETED)    result += "  " + tr("deleted:") + " " + path + "\n";
        if (flags & GIT_STATUS_INDEX_RENAMED)    result += "  " + tr("renamed:") + " " + path + "\n";
        if (flags & GIT_STATUS_WT_NEW)           result += "  " + tr("untracked:") + " " + path + "\n";
        if (flags & GIT_STATUS_WT_MODIFIED)      result += "  " + tr("modified:") + " " + path + "\n";
        if (flags & GIT_STATUS_WT_DELETED)       result += "  " + tr("deleted:") + " " + path + "\n";
    }

    git_status_list_free(status_list);
    return result;
}

bool GitManager::stashSave(const QString &message)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    git_signature *sig = createSignature();
    if (!sig) return false;

    int error = git_stash_save(nullptr, m_repo, sig, message.isEmpty() ? nullptr : message.toUtf8().constData(), GIT_STASH_DEFAULT);
    git_signature_free(sig);

    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Stash save error"));
        return false;
    }

    emit repositoryChanged();
    return true;
}

bool GitManager::stashApply(int index)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    int error = git_stash_apply(m_repo, index, nullptr);
    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Stash apply error"));
        return false;
    }

    emit repositoryChanged();
    return true;
}

bool GitManager::stashDrop(int index)
{
    if (!m_repo) {
        setError(tr("Repository not open"));
        return false;
    }

    int error = git_stash_drop(m_repo, index);
    if (error != 0) {
        const git_error *e = git_error_last();
        setError(e ? QString::fromUtf8(e->message) : tr("Stash delete error"));
        return false;
    }

    return true;
}

QStringList GitManager::stashList() const
{
    QStringList result;
    if (!m_repo) return result;

    git_revwalk *walker = nullptr;
    if (git_revwalk_new(&walker, m_repo) != 0) return result;

    git_revwalk_sorting(walker, GIT_SORT_TIME);
    git_revwalk_push_ref(walker, "refs/stash");

    git_oid oid;
    while (git_revwalk_next(&oid, walker) == 0) {
        git_commit *commit = nullptr;
        if (git_commit_lookup(&commit, m_repo, &oid) == 0) {
            QString msg = QString::fromUtf8(git_commit_message(commit));
            result.append(msg.trimmed());
            git_commit_free(commit);
        }
    }

    git_revwalk_free(walker);
    return result;
}

QString GitManager::logGraph(int count) const
{
    if (!m_repo) return {};

    git_revwalk *walker = nullptr;
    int error = git_revwalk_new(&walker, m_repo);
    if (error != 0) return {};

    git_revwalk_sorting(walker, GIT_SORT_TIME | GIT_SORT_TOPOLOGICAL);
    git_revwalk_push_head(walker);

    // получаем все ветки для пометок
    QStringList branchList = branches();
    QString currentBranch = this->currentBranch();

    QString result;
    git_oid oid;
    int i = 0;

    while (git_revwalk_next(&oid, walker) == 0 && i < count) {
        git_commit *commit = nullptr;
        if (git_commit_lookup(&commit, m_repo, &oid) != 0) continue;

        const git_signature *sig = git_commit_author(commit);
        QString msg = QString::fromUtf8(git_commit_message(commit)).split('\n').first();
        QString oidStr = QString::fromUtf8(git_oid_tostr_s(&oid));
        QString author = QString::fromUtf8(sig->name);
        QString dateStr = QDateTime::fromSecsSinceEpoch(sig->when.time).toString("yyyy-MM-dd HH:mm");

        // проверяем, указывает ли ветка на этот коммит
        QStringList refs;
        for (const QString &branch : branchList) {
            git_oid branch_oid;
            QString refName = "refs/heads/" + branch;
            if (git_reference_name_to_id(&branch_oid, m_repo, refName.toUtf8().constData()) == 0) {
                if (git_oid_equal(&oid, &branch_oid)) {
                    if (branch == currentBranch) {
                        refs.prepend("* " + branch);
                    } else {
                        refs.append(branch);
                    }
                }
            }
        }

        QString refStr;
        if (!refs.isEmpty()) {
            refStr = " (" + refs.join(", ") + ")";
        }

        result += QString("* %1 %2%3\n  | %4 <%5>\n  | %6\n")
                      .arg(oidStr.left(7))
                      .arg(dateStr)
                      .arg(refStr)
                      .arg(author)
                      .arg(QString::fromUtf8(sig->email))
                      .arg(msg);

        git_commit_free(commit);
        i++;
    }

    git_revwalk_free(walker);
    return result;
}

// Приватные методы

QString GitManager::userName() const
{
    if (!m_repo) return "User";

    git_config *cfg = nullptr;
    if (git_repository_config(&cfg, m_repo) != 0) return "User";

    const char *name = nullptr;
    if (git_config_get_string(&name, cfg, "user.name") != 0) {
        git_config_free(cfg);
        return "User";
    }

    QString result = QString::fromUtf8(name);
    git_config_free(cfg);
    return result;
}

QString GitManager::userEmail() const
{
    if (!m_repo) return "user@example.com";

    git_config *cfg = nullptr;
    if (git_repository_config(&cfg, m_repo) != 0) return "user@example.com";

    const char *email = nullptr;
    if (git_config_get_string(&email, cfg, "user.email") != 0) {
        git_config_free(cfg);
        return "user@example.com";
    }

    QString result = QString::fromUtf8(email);
    git_config_free(cfg);
    return result;
}

git_signature *GitManager::createSignature() const
{
    QString name = userName();
    QString email = userEmail();

    git_signature *sig = nullptr;
    int error = git_signature_now(&sig, name.toUtf8().constData(), email.toUtf8().constData());
    if (error != 0) {
        setError(tr("Failed to create signature"));
        return nullptr;
    }

    return sig;
}
