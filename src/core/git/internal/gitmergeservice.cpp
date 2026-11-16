#include "gitmergeservice.h"

#include "gitindexservice.h"
#include "gitrepository.h"

#include <QCoreApplication>
#include <git2.h>

namespace GitInternal {
    namespace {

        QString tr(const char* text) {
            return QCoreApplication::translate("GitManager", text);
        }

        bool failWithLastError(Repository& repository, const QString& fallback) {
            const git_error* error = git_error_last();
            repository.setError(error ? QString::fromUtf8(error->message) : fallback);
            return false;
        }

    }// namespace

    bool MergeService::merge(Repository& repository, const QString& branchName) {
        if (!repository.isOpen()) {
            repository.setError(tr("Repository not open"));
            return false;
        }

        git_oid mergeOid;
        const QString referenceName = branchName.startsWith("refs/")
                                          ? branchName
                                          : QStringLiteral("refs/heads/") + branchName;
        if (git_reference_name_to_id(&mergeOid, repository.handle(), referenceName.toUtf8().constData()) != 0) {
            repository.setError(tr("Failed to find: %1").arg(branchName));
            return false;
        }

        git_annotated_commit* annotatedCommit = nullptr;
        if (git_annotated_commit_lookup(&annotatedCommit, repository.handle(), &mergeOid) != 0)
            return failWithLastError(repository, tr("Failed to find commit for merge"));

        const git_annotated_commit* mergeHeads[] = {annotatedCommit};
        git_merge_options mergeOptions = GIT_MERGE_OPTIONS_INIT;
        git_checkout_options checkoutOptions = GIT_CHECKOUT_OPTIONS_INIT;
        checkoutOptions.checkout_strategy = GIT_CHECKOUT_SAFE;

        const int mergeError = git_merge(repository.handle(), mergeHeads, 1, &mergeOptions, &checkoutOptions);
        git_annotated_commit_free(annotatedCommit);
        if (mergeError != 0)
            return failWithLastError(repository, tr("Merge error"));

        if (IndexService::hasConflicts(repository))
            return true;

        git_index* index = nullptr;
        if (git_repository_index(&index, repository.handle()) != 0)
            return failWithLastError(repository, tr("Failed to get index"));

        git_oid treeOid;
        const int writeTreeError = git_index_write_tree(&treeOid, index);
        git_index_free(index);
        if (writeTreeError != 0)
            return failWithLastError(repository, tr("Failed to write tree"));

        git_tree* tree = nullptr;
        git_commit* headCommit = nullptr;
        git_commit* mergedCommit = nullptr;
        git_signature* signature = nullptr;

        int error = git_tree_lookup(&tree, repository.handle(), &treeOid);
        git_oid headOid;
        if (error == 0)
            error = git_reference_name_to_id(&headOid, repository.handle(), "HEAD");
        if (error == 0)
            error = git_commit_lookup(&headCommit, repository.handle(), &headOid);
        if (error == 0)
            error = git_commit_lookup(&mergedCommit, repository.handle(), &mergeOid);
        if (error == 0) {
            signature = repository.createSignature();
            if (!signature)
                error = -1;
        }

        if (error == 0) {
            const git_commit* parents[] = {headCommit, mergedCommit};
            git_oid commitOid;
            const QByteArray message = tr("Merge branch '%1'").arg(branchName).toUtf8();
            error = git_commit_create(&commitOid,
                                      repository.handle(),
                                      "HEAD",
                                      signature,
                                      signature,
                                      nullptr,
                                      message.constData(),
                                      tree,
                                      2,
                                      parents);
        }

        git_signature_free(signature);
        git_commit_free(mergedCommit);
        git_commit_free(headCommit);
        git_tree_free(tree);

        if (error != 0)
            return failWithLastError(repository, tr("Failed to create merge commit"));

        git_repository_state_cleanup(repository.handle());
        return true;
    }

}// namespace GitInternal
