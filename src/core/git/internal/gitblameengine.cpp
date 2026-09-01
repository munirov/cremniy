#include "gitblameengine.h"

#include "gitrepository.h"

#include <QCoreApplication>
#include <QDateTime>
#include <git2.h>

namespace {
    QString gitTr(const char* text) {
        return QCoreApplication::translate("GitManager", text);
    }
}// namespace

namespace GitInternal {

    QVector<BlameLineInfo> BlameEngine::blameFile(Repository& repository,
                                                  const QString& relativeFilePath) {
        QVector<BlameLineInfo> result;
        if (!repository.isOpen())
            return result;

        git_blame_options options = GIT_BLAME_OPTIONS_INIT;
        options.flags = GIT_BLAME_TRACK_COPIES_SAME_FILE;

        QString path = relativeFilePath;
        if (path.startsWith(QStringLiteral("./")))
            path.remove(0, 2);

        git_blame* blame = nullptr;
        const int error = git_blame_file(
            &blame, repository.handle(), path.toUtf8().constData(), &options);
        if (error != 0) {
            const git_error* gitError = git_error_last();
            repository.setError(gitError ? QString::fromUtf8(gitError->message)
                                         : gitTr("Blame error"));
            return result;
        }

        const uint32_t hunkCount = git_blame_get_hunk_count(blame);
        uint32_t totalLines = 0;
        for (uint32_t i = 0; i < hunkCount; ++i) {
            const git_blame_hunk* hunk = git_blame_get_hunk_byindex(blame, i);
            totalLines = qMax(
                totalLines,
                static_cast<uint32_t>(hunk->final_start_line_number + hunk->lines_in_hunk - 1));
        }
        result.resize(totalLines);

        for (uint32_t i = 0; i < hunkCount; ++i) {
            const git_blame_hunk* hunk = git_blame_get_hunk_byindex(blame, i);
            BlameLineInfo info;

            if (git_oid_is_zero(&hunk->final_commit_id)) {
                info.isUncommitted = true;
                info.authorName = gitTr("You");
                info.commitSummary = gitTr("Uncommitted changes");
            }
            else {
                git_commit* commit = nullptr;
                if (git_commit_lookup(&commit, repository.handle(), &hunk->final_commit_id) == 0) {
                    const git_signature* signature = git_commit_author(commit);
                    info.authorName = QString::fromUtf8(signature->name);
                    info.authorEmail = QString::fromUtf8(signature->email);
                    info.commitDate = QDateTime::fromSecsSinceEpoch(signature->when.time);
                    info.fullOid = QString::fromUtf8(git_oid_tostr_s(&hunk->final_commit_id));
                    info.shortOid = info.fullOid.left(7);
                    info.commitSummary = QString::fromUtf8(git_commit_message(commit)).split('\n').first();
                    git_commit_free(commit);
                }
            }

            for (uint32_t j = 0; j < hunk->lines_in_hunk; ++j) {
                const uint32_t lineIndex = static_cast<uint32_t>(
                    hunk->final_start_line_number + j - 1);
                if (lineIndex < static_cast<uint32_t>(result.size()))
                    result[lineIndex] = info;
            }
        }

        git_blame_free(blame);
        repository.clearError();
        return result;
    }

}// namespace GitInternal
