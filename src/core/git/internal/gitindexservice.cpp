#include "gitindexservice.h"

#include "gitrepository.h"

#include <QCoreApplication>
#include <git2.h>

namespace {
    QString gitTr(const char* text) {
        return QCoreApplication::translate("GitManager", text);
    }

    QString diffToString(git_diff* diff) {
        git_buf buffer = GIT_BUF_INIT;
        const int error = git_diff_to_buf(&buffer, diff, GIT_DIFF_FORMAT_PATCH);
        git_diff_free(diff);
        if (error != 0) {
            git_buf_dispose(&buffer);
            return {};
        }

        const QString result = QString::fromUtf8(buffer.ptr, buffer.size);
        git_buf_dispose(&buffer);
        return result;
    }
}// namespace

namespace GitInternal {

    bool IndexService::hasConflicts(const Repository& repository) {
        if (!repository.isOpen())
            return false;

        git_index* index = nullptr;
        if (git_repository_index(&index, repository.handle()) != 0)
            return false;
        const bool result = git_index_has_conflicts(index);
        git_index_free(index);
        return result;
    }

    QStringList IndexService::conflictFiles(const Repository& repository) {
        QStringList result;
        if (!repository.isOpen())
            return result;

        git_index* index = nullptr;
        if (git_repository_index(&index, repository.handle()) != 0)
            return result;

        git_index_conflict_iterator* iterator = nullptr;
        if (git_index_conflict_iterator_new(&iterator, index) != 0) {
            git_index_free(index);
            return result;
        }

        const git_index_entry* ancestor = nullptr;
        const git_index_entry* ours = nullptr;
        const git_index_entry* theirs = nullptr;
        while (git_index_conflict_next(&ancestor, &ours, &theirs, iterator) == 0) {
            if (ours)
                result.append(QString::fromUtf8(ours->path));
            else if (theirs)
                result.append(QString::fromUtf8(theirs->path));
        }

        git_index_conflict_iterator_free(iterator);
        git_index_free(index);
        return result;
    }

    bool IndexService::stage(Repository& repository, const QString& filePath) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }

        git_index* index = nullptr;
        if (git_repository_index(&index, repository.handle()) != 0) {
            repository.setError(gitTr("Failed to get index"));
            return false;
        }

        int error = git_index_add_bypath(index, filePath.toUtf8().constData());
        if (error == 0)
            error = git_index_write(index);
        git_index_free(index);

        if (error != 0) {
            const git_error* gitError = git_error_last();
            repository.setError(gitError ? QString::fromUtf8(gitError->message)
                                         : gitTr("Failed to add file"));
            return false;
        }

        repository.clearError();
        return true;
    }

    bool IndexService::unstage(Repository& repository, const QString& filePath) {
        if (!repository.isOpen()) {
            repository.setError(gitTr("Repository not open"));
            return false;
        }

        git_index* index = nullptr;
        if (git_repository_index(&index, repository.handle()) != 0) {
            repository.setError(gitTr("Failed to get index"));
            return false;
        }

        int error = git_index_remove_bypath(index, filePath.toUtf8().constData());
        if (error == 0)
            error = git_index_write(index);
        git_index_free(index);

        if (error != 0) {
            const git_error* gitError = git_error_last();
            repository.setError(gitError ? QString::fromUtf8(gitError->message)
                                         : gitTr("Failed to remove file from index"));
            return false;
        }

        repository.clearError();
        return true;
    }

    QString IndexService::fileDiff(const Repository& repository, const QString& filePath) {
        if (!repository.isOpen())
            return {};

        git_diff_options options = GIT_DIFF_OPTIONS_INIT;
        options.flags = GIT_DIFF_INCLUDE_UNTRACKED;
        const QByteArray encodedPath = filePath.toUtf8();
        char* path = const_cast<char*>(encodedPath.constData());
        if (!filePath.isEmpty())
            options.pathspec = git_strarray{&path, 1};

        git_diff* diff = nullptr;
        if (git_diff_index_to_workdir(&diff, repository.handle(), nullptr, &options) != 0)
            return {};
        return diffToString(diff);
    }

    QString IndexService::stagedDiff(const Repository& repository) {
        if (!repository.isOpen())
            return {};

        git_diff* diff = nullptr;
        git_diff_options options = GIT_DIFF_OPTIONS_INIT;
        if (git_diff_tree_to_index(
                &diff, repository.handle(), nullptr, nullptr, &options)
            != 0)
            return {};
        return diffToString(diff);
    }

    QString IndexService::status(const Repository& repository, const QString& currentBranch) {
        if (!repository.isOpen())
            return {};

        git_status_options options = GIT_STATUS_OPTIONS_INIT;
        options.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED
                        | GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX
                        | GIT_STATUS_OPT_SORT_CASE_SENSITIVELY;

        git_status_list* statusList = nullptr;
        if (git_status_list_new(&statusList, repository.handle(), &options) != 0)
            return {};

        QString result = gitTr("On branch %1\n")
                             .arg(currentBranch.isEmpty() ? QStringLiteral("HEAD") : currentBranch);
        const size_t count = git_status_list_entrycount(statusList);
        for (size_t i = 0; i < count; ++i) {
            const git_status_entry* entry = git_status_byindex(statusList, i);
            if (!entry)
                continue;

            QString path;
            if (entry->head_to_index)
                path = QString::fromUtf8(entry->head_to_index->old_file.path);
            else if (entry->index_to_workdir)
                path = QString::fromUtf8(entry->index_to_workdir->old_file.path);

            const unsigned int flags = entry->status;
            if (flags & GIT_STATUS_INDEX_NEW)
                result += gitTr("  new:    %1\n").arg(path);
            if (flags & GIT_STATUS_INDEX_MODIFIED)
                result += gitTr("  modified: %1\n").arg(path);
            if (flags & GIT_STATUS_INDEX_DELETED)
                result += gitTr("  deleted: %1\n").arg(path);
            if (flags & GIT_STATUS_INDEX_RENAMED)
                result += gitTr("  renamed: %1\n").arg(path);
            if (flags & GIT_STATUS_WT_NEW)
                result += gitTr("  untracked: %1\n").arg(path);
            if (flags & GIT_STATUS_WT_MODIFIED)
                result += gitTr("  modified: %1\n").arg(path);
            if (flags & GIT_STATUS_WT_DELETED)
                result += gitTr("  deleted: %1\n").arg(path);
        }

        git_status_list_free(statusList);
        return result;
    }

}// namespace GitInternal
