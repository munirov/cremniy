#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

enum class SearchScope {
    CurrentFile,
    OpenFiles,
    Project
};

struct SearchOptions {
    QString query;
    QString replacement;
    SearchScope scope = SearchScope::CurrentFile;
    bool caseSensitive = false;
    bool wholeWord = false;
    bool regularExpression = false;
    QString includePattern;
    QString excludePattern;
    QStringList excludedPatterns;
    int maximumResults = 10000;
    qint64 maximumFileSize = 16 * 1024 * 1024;
};

struct SearchDocument {
    QString filePath;
    QByteArray contents;
};

struct SearchMatch {
    QString filePath;
    int line = 1;
    int column = 0;
    int length = 0;
    int textOffset = 0;
    QString preview;
    QString replacementText;
};

struct SearchReport {
    QVector<SearchMatch> matches;
    int filesSearched = 0;
    int filesSkipped = 0;
    bool truncated = false;
    QString error;
};

struct ReplacementResult {
    QByteArray contents;
    int replacements = 0;
    QString error;
};
