#include "gitstashservice.h"

#include "gitrepository.h"

#include <QCoreApplication>
#include <git2.h>

namespace GitInternal {
    namespace {

        QString tr(const char* text) {
            return QCoreApplication::translate("GitManager", text);
        }

        bool ensureOpen(Repository& repository) {
            if (repository.isOpen())
                return true;

            repository.setError(tr("Repository not open"));
            return false;
        }

        bool failWithLastError(Repository& repository, const QString& fallback) {
            const git_error* error = git_error_last();
            repository.setError(error ? QString::fromUtf8(error->message) : fallback);
            return false;
        }

        int collectStash(size_t, const char* message, const git_oid*, void* payload) {
            auto* result = static_cast<QStringList*>(payload);
            result->append(QString::fromUtf8(message).trimmed());
            return 0;
        }

    }// namespace

    bool StashService::save(Repository& repository, const QString& message) {
        if (!ensureOpen(repository))
            return false;

        git_signature* signature = repository.createSignature();
        if (!signature)
            return false;

        const QByteArray encodedMessage = message.toUtf8();
        const int error = git_stash_save(nullptr,
                                         repository.handle(),
                                         signature,
                                         message.isEmpty() ? nullptr : encodedMessage.constData(),
                                         GIT_STASH_DEFAULT);
        git_signature_free(signature);

        if (error != 0)
            return failWithLastError(repository, tr("Stash save error"));
        return true;
    }

    bool StashService::apply(Repository& repository, int index) {
        if (!ensureOpen(repository))
            return false;
        if (git_stash_apply(repository.handle(), index, nullptr) != 0)
            return failWithLastError(repository, tr("Stash apply error"));
        return true;
    }

    bool StashService::drop(Repository& repository, int index) {
        if (!ensureOpen(repository))
            return false;
        if (git_stash_drop(repository.handle(), index) != 0)
            return failWithLastError(repository, tr("Stash delete error"));
        return true;
    }

    QStringList StashService::list(const Repository& repository) {
        QStringList result;
        if (!repository.isOpen())
            return result;

        git_stash_foreach(repository.handle(), collectStash, &result);
        return result;
    }

}// namespace GitInternal
