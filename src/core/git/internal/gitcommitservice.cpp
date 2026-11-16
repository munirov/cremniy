#include "gitcommitservice.h"

#include "gitbranchservice.h"
#include "gitrepository.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QVector>
#include <git2.h>
#include <utility>

namespace {
    QString gitTr(const char* text) {
        return QCoreApplication::translate("GitManager", text);
    }

    QString gitErrorOr(const char* fallback) {
        const git_error* error = git_error_last();
        return error ? QString::fromUtf8(error->message) : gitTr(fallback);
    }
}// namespace

namespace GitInternal {

    bool CommitService::create(Repository& repository, const QString& message) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }

        git_index* index = nullptr;
        if (git_repository_index(&index, repository.handle()) != 0) {
            repository.setError(gitTr("Failed to get index"));
            return false;
        }

        git_oid treeOid;
        int error = git_index_write_tree(&treeOid, index);
        if (error != 0) {
            git_index_free(index);
            repository.setError(gitTr("Failed to write tree"));
            return false;
        }

        git_tree* tree = nullptr;
        error = git_tree_lookup(&tree, repository.handle(), &treeOid);
        git_index_free(index);
        if (error != 0) {
            repository.setError(gitTr("Failed to find tree"));
            return false;
        }

        git_commit* parent = nullptr;
        git_oid headOid;
        if (git_reference_name_to_id(&headOid, repository.handle(), "HEAD") == 0)
            git_commit_lookup(&parent, repository.handle(), &headOid);

        git_signature* signature = repository.createSignature();
        if (!signature) {
            git_tree_free(tree);
            if (parent)
                git_commit_free(parent);
            return false;
        }

        git_oid commitOid;
        const QByteArray encodedMessage = message.toUtf8();
        if (parent) {
            const git_commit* parents[] = {parent};
            error = git_commit_create(
                &commitOid, repository.handle(), "HEAD", signature, signature, nullptr,
                encodedMessage.constData(), tree, 1, parents);
        }
        else {
            error = git_commit_create(
                &commitOid, repository.handle(), "HEAD", signature, signature, nullptr,
                encodedMessage.constData(), tree, 0, nullptr);
        }

        git_signature_free(signature);
        git_tree_free(tree);
        if (parent)
            git_commit_free(parent);

        if (error != 0) {
            repository.setError(gitErrorOr("Failed to create commit"));
            return false;
        }

        repository.clearError();
        return true;
    }

    QStringList CommitService::history(const Repository& repository, int count) {
        QStringList result;
        if (!repository.isOpen())
            return result;

        git_revwalk* walker = nullptr;
        if (git_revwalk_new(&walker, repository.handle()) != 0)
            return result;

        git_revwalk_sorting(walker, GIT_SORT_TIME);
        git_revwalk_push_head(walker);
        git_oid oid;
        while (result.size() < count && git_revwalk_next(&oid, walker) == 0)
            result.append(QString::fromUtf8(git_oid_tostr_s(&oid)));
        git_revwalk_free(walker);
        return result;
    }

    QString CommitService::message(const Repository& repository, const QString& oid) {
        if (!repository.isOpen())
            return {};

        git_oid commitOid;
        if (git_oid_fromstr(&commitOid, oid.toUtf8().constData()) != 0)
            return {};

        git_commit* commit = nullptr;
        if (git_commit_lookup(&commit, repository.handle(), &commitOid) != 0)
            return {};

        const QString result = QString::fromUtf8(git_commit_message(commit)).trimmed();
        git_commit_free(commit);
        return result;
    }

    QString CommitService::author(const Repository& repository, const QString& oid) {
        if (!repository.isOpen())
            return {};

        git_oid commitOid;
        if (git_oid_fromstr(&commitOid, oid.toUtf8().constData()) != 0)
            return {};

        git_commit* commit = nullptr;
        if (git_commit_lookup(&commit, repository.handle(), &commitOid) != 0)
            return {};

        const git_signature* signature = git_commit_author(commit);
        const QString result = QString::fromUtf8(signature->name)
                               + QStringLiteral(" <") + QString::fromUtf8(signature->email) + QLatin1Char('>');
        git_commit_free(commit);
        return result;
    }

    bool CommitService::checkout(Repository& repository, const QString& oid) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }

        git_oid commitOid;
        if (git_oid_fromstr(&commitOid, oid.toUtf8().constData()) != 0) {
            repository.setError(gitTr("Invalid commit OID"));
            return false;
        }

        git_commit* commit = nullptr;
        int error = git_commit_lookup(&commit, repository.handle(), &commitOid);
        if (error != 0) {
            repository.setError(gitTr("Commit not found"));
            return false;
        }

        git_checkout_options options = GIT_CHECKOUT_OPTIONS_INIT;
        options.checkout_strategy = GIT_CHECKOUT_SAFE;
        error = git_checkout_tree(
            repository.handle(), reinterpret_cast<const git_object*>(commit), &options);
        if (error == 0)
            error = git_repository_set_head_detached(repository.handle(), &commitOid);
        git_commit_free(commit);

        if (error != 0) {
            repository.setError(gitErrorOr("Checkout error"));
            return false;
        }

        repository.clearError();
        return true;
    }

    bool CommitService::resetHard(Repository& repository, const QString& oid) {
        return reset(repository, oid, GIT_RESET_HARD);
    }

    bool CommitService::resetMixed(Repository& repository, const QString& oid) {
        return reset(repository, oid, GIT_RESET_MIXED);
    }

    bool CommitService::reset(Repository& repository, const QString& oid, int resetType) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }

        git_oid commitOid;
        if (git_oid_fromstr(&commitOid, oid.toUtf8().constData()) != 0) {
            repository.setError(gitTr("Invalid commit OID"));
            return false;
        }

        git_object* target = nullptr;
        if (git_object_lookup(&target, repository.handle(), &commitOid, GIT_OBJECT_COMMIT) != 0) {
            repository.setError(gitTr("Object not found"));
            return false;
        }

        const int error = git_reset(
            repository.handle(), target, static_cast<git_reset_t>(resetType), nullptr);
        git_object_free(target);
        if (error != 0) {
            repository.setError(gitErrorOr("Reset error"));
            return false;
        }

        repository.clearError();
        return true;
    }

    bool CommitService::revert(Repository& repository, const QString& oid) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }

        git_oid commitOid;
        if (git_oid_fromstr(&commitOid, oid.toUtf8().constData()) != 0) {
            repository.setError(gitTr("Invalid commit OID"));
            return false;
        }

        git_commit* commit = nullptr;
        if (git_commit_lookup(&commit, repository.handle(), &commitOid) != 0) {
            repository.setError(gitTr("Commit not found"));
            return false;
        }

        const int error = git_revert(repository.handle(), commit, nullptr);
        git_commit_free(commit);
        if (error != 0) {
            repository.setError(gitErrorOr("Revert error"));
            return false;
        }

        repository.clearError();
        return true;
    }

    bool CommitService::amend(Repository& repository, const QString& message) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }

        git_oid headOid;
        if (git_reference_name_to_id(&headOid, repository.handle(), "HEAD") != 0) {
            repository.setError(gitTr("Failed to get HEAD"));
            return false;
        }

        git_commit* oldCommit = nullptr;
        if (git_commit_lookup(&oldCommit, repository.handle(), &headOid) != 0) {
            repository.setError(gitTr("Failed to find HEAD commit"));
            return false;
        }

        git_signature* signature = repository.createSignature();
        if (!signature) {
            git_commit_free(oldCommit);
            return false;
        }

        git_tree* tree = nullptr;
        if (git_commit_tree(&tree, oldCommit) != 0) {
            git_signature_free(signature);
            git_commit_free(oldCommit);
            repository.setError(gitTr("Failed to get commit tree"));
            return false;
        }

        QVector<const git_commit*> parents;
        const unsigned int parentCount = git_commit_parentcount(oldCommit);
        parents.reserve(static_cast<qsizetype>(parentCount));
        for (unsigned int i = 0; i < parentCount; ++i) {
            git_commit* parent = nullptr;
            if (git_commit_parent(&parent, oldCommit, i) == 0)
                parents.append(parent);
        }

        git_oid newCommitOid;
        const QByteArray encodedMessage = message.toUtf8();
        const int error = git_commit_create(
            &newCommitOid, repository.handle(), "HEAD", signature, signature, nullptr,
            encodedMessage.constData(), tree, static_cast<size_t>(parents.size()),
            parents.isEmpty() ? nullptr : parents.data());

        for (const git_commit* parent: std::as_const(parents))
            git_commit_free(const_cast<git_commit*>(parent));
        git_signature_free(signature);
        git_tree_free(tree);
        git_commit_free(oldCommit);

        if (error != 0) {
            repository.setError(gitErrorOr("Amend error"));
            return false;
        }

        repository.clearError();
        return true;
    }

    QString CommitService::graph(const Repository& repository, int count) {
        if (!repository.isOpen() || count <= 0)
            return {};

        git_revwalk* walker = nullptr;
        if (git_revwalk_new(&walker, repository.handle()) != 0)
            return {};

        git_revwalk_sorting(walker, GIT_SORT_TIME | GIT_SORT_TOPOLOGICAL);
        if (git_revwalk_push_head(walker) != 0) {
            git_revwalk_free(walker);
            return {};
        }

        const QStringList branchList = BranchService::branches(repository);
        const QString currentBranch = BranchService::currentBranch(repository);

        QString result;
        git_oid oid;
        int commitCount = 0;
        while (commitCount < count && git_revwalk_next(&oid, walker) == 0) {
            git_commit* commit = nullptr;
            if (git_commit_lookup(&commit, repository.handle(), &oid) != 0)
                continue;

            const git_signature* signature = git_commit_author(commit);
            const QString message = QString::fromUtf8(git_commit_message(commit)).split('\n').first();
            const QString oidString = QString::fromUtf8(git_oid_tostr_s(&oid));
            const QString author = signature && signature->name
                                       ? QString::fromUtf8(signature->name)
                                       : QString();
            const QString email = signature && signature->email
                                      ? QString::fromUtf8(signature->email)
                                      : QString();
            const QString date = signature
                                     ? QDateTime::fromSecsSinceEpoch(signature->when.time).toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                     : QString();

            QStringList references;
            for (const QString& branch: branchList) {
                git_oid branchOid;
                const QByteArray referenceName = (QStringLiteral("refs/heads/") + branch).toUtf8();
                if (git_reference_name_to_id(&branchOid, repository.handle(), referenceName.constData()) == 0
                    && git_oid_equal(&oid, &branchOid)) {
                    if (branch == currentBranch)
                        references.prepend(QStringLiteral("* ") + branch);
                    else
                        references.append(branch);
                }
            }

            const QString referenceText = references.isEmpty()
                                              ? QString()
                                              : QStringLiteral(" (") + references.join(QStringLiteral(", ")) + QStringLiteral(")");
            result += gitTr("* %1 %2%3\n  | %4 <%5>\n  | %6\n")
                          .arg(oidString.left(7), date, referenceText, author, email, message);

            git_commit_free(commit);
            ++commitCount;
        }

        git_revwalk_free(walker);
        return result;
    }

}// namespace GitInternal
