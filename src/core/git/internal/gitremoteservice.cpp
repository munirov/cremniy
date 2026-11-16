#include "gitremoteservice.h"

#include "gitbranchservice.h"
#include "gitmergeservice.h"
#include "gitrepository.h"

#include <QCoreApplication>
#include <git2.h>

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

    bool RemoteService::push(Repository& repository,
                             const QString& remote,
                             const QString& branch) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }
        const QString resolvedBranch = branch.isEmpty()
                                           ? BranchService::currentBranch(repository)
                                           : branch;
        if (resolvedBranch.isEmpty()) {
            repository.setError(gitTr("Push branch not specified"));
            return false;
        }

        git_remote* gitRemote = nullptr;
        if (git_remote_lookup(&gitRemote, repository.handle(), remote.toUtf8().constData()) != 0) {
            repository.setError(gitTr("Remote not found: %1").arg(remote));
            return false;
        }

        const QByteArray refspec = (QStringLiteral("refs/heads/") + resolvedBranch
                                    + QStringLiteral(":refs/heads/") + resolvedBranch)
                                       .toUtf8();
        char* refspecValue = const_cast<char*>(refspec.constData());
        git_strarray refspecs{&refspecValue, 1};
        git_push_options options = GIT_PUSH_OPTIONS_INIT;
        const int error = git_remote_push(gitRemote, &refspecs, &options);
        git_remote_free(gitRemote);

        if (error != 0) {
            repository.setError(gitErrorOr("Push error"));
            return false;
        }

        repository.clearError();
        return true;
    }

    bool RemoteService::pull(Repository& repository,
                             const QString& remote,
                             const QString& branch) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }
        if (!fetch(repository, remote))
            return false;

        const QString resolvedBranch = branch.isEmpty()
                                           ? BranchService::currentBranch(repository)
                                           : branch;
        if (resolvedBranch.isEmpty()) {
            repository.setError(gitTr("Branch not specified"));
            return false;
        }

        const QString remoteReference = QStringLiteral("refs/remotes/") + remote
                                        + QStringLiteral("/") + resolvedBranch;
        return MergeService::merge(repository, remoteReference);
    }

    bool RemoteService::fetch(Repository& repository, const QString& remote) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }

        git_remote* gitRemote = nullptr;
        if (git_remote_lookup(&gitRemote, repository.handle(), remote.toUtf8().constData()) != 0) {
            repository.setError(gitTr("Remote not found: %1").arg(remote));
            return false;
        }

        git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
        const int error = git_remote_fetch(gitRemote, nullptr, &options, nullptr);
        git_remote_free(gitRemote);
        if (error != 0) {
            repository.setError(gitErrorOr("Fetch error"));
            return false;
        }

        repository.clearError();
        return true;
    }

}// namespace GitInternal
