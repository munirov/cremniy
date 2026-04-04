#ifndef PROJECTSEARCHENGINE_H
#define PROJECTSEARCHENGINE_H

#include <QRegularExpression>
#include <QString>
#include <optional>

struct ProjectSearchOptions {
    QString query;
    bool caseSensitive = false;
    bool wholeWord = false;
    bool useRegex = false;
};

struct ProjectSearchHit {
    QString filePath;
    int lineNumber = 0;
    QString linePreview;
};

namespace ProjectSearchEngine {

bool isProbableBinarySample(const QByteArray &sample);

bool shouldSkipDirectoryName(const QString &name);

bool pathContainsSkippedDirectory(const QString &rootPath,
                                  const QString &absoluteFilePath);

std::optional<QRegularExpression> buildLineMatcher(const ProjectSearchOptions &opt,
                                                   QString *errorMessage = nullptr);

bool lineMatches(const QString &line, const QRegularExpression &re);

} // namespace ProjectSearchEngine

#endif
