#include "gitbranchservice.h"

#include "gitrepository.h"

#include <QCoreApplication>
#include <git2.h>

namespace {
    QString gitTr(const char* text) {
        return QCoreApplication::translate("GitManager", text);
    }
}// namespace

namespace GitInternal {

    QStringList BranchService::branches(const Repository& repository) {
        QStringList result;
        if (!repository.isOpen())
            return result;

        git_branch_iterator* iterator = nullptr;
        if (git_branch_iterator_new(&iterator, repository.handle(), GIT_BRANCH_LOCAL) != 0)
            return result;

        git_reference* reference = nullptr;
        git_branch_t type;
        while (git_branch_next(&reference, &type, iterator) == 0) {
            const char* name = nullptr;
            if (git_branch_name(&name, reference) == 0)
                result.append(QString::fromUtf8(name));
            git_reference_free(reference);
        }
        git_branch_iterator_free(iterator);
        return result;
    }

    QString BranchService::currentBranch(const Repository& repository) {
        if (!repository.isOpen())
            return {};

        git_reference* head = nullptr;
        if (git_repository_head(&head, repository.handle()) != 0)
            return {};

        const char* name = nullptr;
        git_branch_name(&name, head);
        const QString branchName = QString::fromUtf8(name);
        git_reference_free(head);
        return branchName;
    }

    bool BranchService::checkout(Repository& repository, const QString& branchName) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }

        git_reference* reference = nullptr;
        const QString referenceName = QStringLiteral("refs/heads/") + branchName;
        int error = git_reference_dwim(
            &reference, repository.handle(), referenceName.toUtf8().constData());
        if (error != 0) {
            repository.setError(gitTr("Branch not found: %1").arg(branchName));
            return false;
        }

        git_checkout_options options = GIT_CHECKOUT_OPTIONS_INIT;
        options.checkout_strategy = GIT_CHECKOUT_SAFE;
        error = git_checkout_tree(
            repository.handle(), reinterpret_cast<const git_object*>(git_reference_target(reference)), &options);
        if (error != 0) {
            git_reference_free(reference);
            const git_error* gitError = git_error_last();
            repository.setError(
                gitError ? QString::fromUtf8(gitError->message) : gitTr("Checkout error"));
            return false;
        }

        error = git_repository_set_head(repository.handle(), referenceName.toUtf8().constData());
        git_reference_free(reference);
        if (error != 0) {
            const git_error* gitError = git_error_last();
            repository.setError(
                gitError ? QString::fromUtf8(gitError->message) : gitTr("HEAD setup error"));
            return false;
        }

        repository.clearError();
        return true;
    }

    bool BranchService::create(Repository& repository, const QString& branchName) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }

        git_oid headOid;
        if (git_reference_name_to_id(&headOid, repository.handle(), "HEAD") != 0) {
            repository.setError(gitTr("Failed to get HEAD"));
            return false;
        }

        git_commit* commit = nullptr;
        if (git_commit_lookup(&commit, repository.handle(), &headOid) != 0) {
            repository.setError(gitTr("Failed to find HEAD commit"));
            return false;
        }

        git_reference* reference = nullptr;
        const int error = git_branch_create(
            &reference, repository.handle(), branchName.toUtf8().constData(), commit, 0);
        git_commit_free(commit);
        if (error != 0) {
            const git_error* gitError = git_error_last();
            repository.setError(
                gitError ? QString::fromUtf8(gitError->message) : gitTr("Failed to create branch"));
            return false;
        }

        git_reference_free(reference);
        repository.clearError();
        return true;
    }

    bool BranchService::remove(Repository& repository, const QString& branchName) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }

        git_reference* reference = nullptr;
        const QString referenceName = QStringLiteral("refs/heads/") + branchName;
        if (git_reference_dwim(
                &reference, repository.handle(), referenceName.toUtf8().constData())
            != 0) {
            repository.setError(gitTr("Branch not found: %1").arg(branchName));
            return false;
        }

        const int error = git_branch_delete(reference);
        git_reference_free(reference);
        if (error != 0) {
            const git_error* gitError = git_error_last();
            repository.setError(
                gitError ? QString::fromUtf8(gitError->message) : gitTr("Failed to delete branch"));
            return false;
        }

        repository.clearError();
        return true;
    }

    bool BranchService::rename(Repository& repository,
                               const QString& oldName,
                               const QString& newName) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }

        git_reference* reference = nullptr;
        const QString referenceName = QStringLiteral("refs/heads/") + oldName;
        if (git_reference_dwim(
                &reference, repository.handle(), referenceName.toUtf8().constData())
            != 0) {
            repository.setError(gitTr("Branch not found: %1").arg(oldName));
            return false;
        }

        git_reference* renamedReference = nullptr;
        const int error = git_branch_move(
            &renamedReference, reference, newName.toUtf8().constData(), 0);
        git_reference_free(reference);
        if (error != 0) {
            const git_error* gitError = git_error_last();
            repository.setError(
                gitError ? QString::fromUtf8(gitError->message) : gitTr("Failed to rename branch"));
            return false;
        }

        git_reference_free(renamedReference);
        repository.clearError();
        return true;
    }

}// namespace GitInternal
