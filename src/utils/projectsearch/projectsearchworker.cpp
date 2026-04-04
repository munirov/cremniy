#include "projectsearchworker.h"
#include "projectsearchengine.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace {

constexpr int kBatchSize = 80;

QString decodeUtf8LineBody(const QByteArray &data, qint64 start, qint64 endExclusive)
{
    if (endExclusive <= start)
        return {};
    QByteArray body = data.mid(static_cast<int>(start), static_cast<int>(endExclusive - start));
    if (start == 0 && body.size() >= 3 && static_cast<uchar>(body[0]) == 0xEF && static_cast<uchar>(body[1]) == 0xBB &&
        static_cast<uchar>(body[2]) == 0xBF) {
        body = body.mid(3);
    }
    return QString::fromUtf8(body);
}

void pushHit(const QString &line, const QString &needle, const QString &filePath, QStringList &batchPaths,
             QList<int> &batchLines, QStringList &batchPreviews, int lineNumber)
{
    if (line.contains(needle, Qt::CaseInsensitive)) {
        batchPaths.append(filePath);
        batchLines.append(lineNumber);
        batchPreviews.append(line);
    }
}

} // namespace

void ProjectSearchWorker::requestCancel()
{
    m_cancelled.store(true);
}

ProjectSearchWorker::ProjectSearchWorker(QObject *parent) : QObject(parent) {}

void ProjectSearchWorker::runSearch(QString rootPath, QString query)
{
    m_cancelled.store(false);

    const QString needle = query.trimmed();
    if (needle.isEmpty()) {
        emit searchFinished();
        return;
    }

    QStringList batchPaths;
    QList<int> batchLines;
    QStringList batchPreviews;

    auto flushBatch = [this, &batchPaths, &batchLines, &batchPreviews]() {
        if (batchPaths.isEmpty())
            return;
        emit hitsBatch(batchPaths, batchLines, batchPreviews);
        batchPaths.clear();
        batchLines.clear();
        batchPreviews.clear();
    };

    const QString rootClean = QDir::cleanPath(QFileInfo(rootPath).absoluteFilePath());
    QDirIterator it(rootClean, QDir::Files | QDir::Readable | QDir::NoSymLinks, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        if (m_cancelled.load())
            break;

        const QString filePath = it.next();
        if (ProjectSearchEngine::pathContainsSkippedDirectory(rootClean, filePath))
            continue;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            continue;

        constexpr qint64 kProbe = 4096;
        const QByteArray probe = file.peek(kProbe);
        if (ProjectSearchEngine::isProbableBinarySample(probe)) {
            file.close();
            continue;
        }
        file.seek(0);

        const QByteArray fileData = file.readAll();
        file.close();

        const qint64 bufferSize = fileData.size();
        qint64 lineStart = 0;
        int lineNumber = 0;

        for (qint64 absolutePos = 0; absolutePos < bufferSize; ++absolutePos) {
            if (m_cancelled.load())
                break;

            const char c = fileData[static_cast<int>(absolutePos)];
            if (c == '\n') {
                ++lineNumber;
                const QString line = decodeUtf8LineBody(fileData, lineStart, absolutePos);
                pushHit(line, needle, filePath, batchPaths, batchLines, batchPreviews, lineNumber);
                if (batchPaths.size() >= kBatchSize)
                    flushBatch();
                lineStart = absolutePos + 1;
            } else if (c == '\r') {
                if (absolutePos + 1 < bufferSize && fileData[static_cast<int>(absolutePos + 1)] == '\n') {
                    ++lineNumber;
                    const QString line = decodeUtf8LineBody(fileData, lineStart, absolutePos);
                    pushHit(line, needle, filePath, batchPaths, batchLines, batchPreviews, lineNumber);
                    if (batchPaths.size() >= kBatchSize)
                        flushBatch();
                    lineStart = absolutePos + 2;
                    ++absolutePos;
                } else {
                    ++lineNumber;
                    const QString line = decodeUtf8LineBody(fileData, lineStart, absolutePos);
                    pushHit(line, needle, filePath, batchPaths, batchLines, batchPreviews, lineNumber);
                    if (batchPaths.size() >= kBatchSize)
                        flushBatch();
                    lineStart = absolutePos + 1;
                }
            }
        }

        if (m_cancelled.load())
            break;

        if (lineStart < bufferSize) {
            ++lineNumber;
            const QString line = decodeUtf8LineBody(fileData, lineStart, bufferSize);
            pushHit(line, needle, filePath, batchPaths, batchLines, batchPreviews, lineNumber);
            if (batchPaths.size() >= kBatchSize)
                flushBatch();
        } else if (lineStart == bufferSize && bufferSize > 0) {
            ++lineNumber;
            pushHit(QString(), needle, filePath, batchPaths, batchLines, batchPreviews, lineNumber);
            if (batchPaths.size() >= kBatchSize)
                flushBatch();
        }
    }

    flushBatch();
    emit searchFinished();
}
