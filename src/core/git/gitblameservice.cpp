#include "gitblameservice.h"

#include "internal/gitblameengine.h"
#include "internal/gitrepository.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrentRun>

namespace {
    struct BlameRequestResult {
        QVector<BlameLineInfo> lines;
        QString error;
    };
}// namespace

GitBlameService* GitBlameService::instance() {
    static GitBlameService service;
    return &service;
}

GitBlameService::GitBlameService(QObject* parent)
    : QObject(parent) {
}

void GitBlameService::requestBlame(const QString& filePath) {
    if (filePath.isEmpty()) {
        emit blameReady({}, {});
        return;
    }

    const QString absolutePath = QFileInfo(filePath).absoluteFilePath();
    const quint64 requestVersion = ++m_nextRequestVersion;
    m_requestVersions.insert(absolutePath, requestVersion);

    auto* watcher = new QFutureWatcher<BlameRequestResult>(this);
    connect(watcher, &QFutureWatcher<BlameRequestResult>::finished,
            this, [this, watcher, absolutePath, requestVersion]() {
                const BlameRequestResult result = watcher->result();
                watcher->deleteLater();

                if (m_requestVersions.value(absolutePath) != requestVersion)
                    return;

                m_requestVersions.remove(absolutePath);
                if (result.error.isEmpty())
                    emit blameReady(absolutePath, result.lines);
                else
                    emit blameFailed(absolutePath, result.error);
            });

    watcher->setFuture(QtConcurrent::run([absolutePath]() {
        BlameRequestResult result;
        const QString repoRoot = GitInternal::Repository::discoverRoot(
            QFileInfo(absolutePath).absolutePath());
        if (repoRoot.isEmpty())
            return result;

        GitInternal::Repository repository;
        if (!repository.open(repoRoot)) {
            result.error = repository.lastError();
            return result;
        }

        const QString relativePath = QDir(repoRoot).relativeFilePath(absolutePath);
        result.lines = GitInternal::BlameEngine::blameFile(repository, relativePath);
        if (result.lines.isEmpty() && !repository.lastError().isEmpty())
            result.error = repository.lastError();
        return result;
    }));
}
