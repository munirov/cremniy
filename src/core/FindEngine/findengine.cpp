#include "findengine.h"

#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>

#include <algorithm>

namespace {

QString searchTr(const char* sourceText)
{
    return QCoreApplication::translate("FindEngine", sourceText);
}

QString normalizedPath(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).replace('\\', '/');
}

QStringList splitPatterns(const QString& value)
{
    QStringList patterns = value.split(QRegularExpression(QStringLiteral("[;,]")), Qt::SkipEmptyParts);
    for (QString& pattern : patterns)
        pattern = pattern.trimmed().replace('\\', '/');
    patterns.removeAll(QString());
    return patterns;
}

bool globMatches(const QString& pattern, const QString& relativePath, const QString& fileName)
{
    const QString target = pattern.contains('/') ? relativePath : fileName;
    const QRegularExpression expression(
        QRegularExpression::wildcardToRegularExpression(pattern),
        QRegularExpression::CaseInsensitiveOption);
    return expression.isValid() && expression.match(target).hasMatch();
}

bool matchesAnyPattern(const QStringList& patterns, const QString& relativePath, const QString& fileName)
{
    return std::any_of(patterns.cbegin(), patterns.cend(), [&](const QString& pattern) {
        return globMatches(pattern, relativePath, fileName);
    });
}

bool shouldIncludeFile(const QString& relativePath, const SearchOptions& options)
{
    const QString fileName = QFileInfo(relativePath).fileName();
    const QStringList includePatterns = splitPatterns(options.includePattern);
    if (!includePatterns.isEmpty() && !matchesAnyPattern(includePatterns, relativePath, fileName))
        return false;

    QStringList excludePatterns = options.excludedPatterns;
    excludePatterns.append(splitPatterns(options.excludePattern));
    return !matchesAnyPattern(excludePatterns, relativePath, fileName);
}

bool shouldSkipDirectory(const QString& relativePath, const QString& directoryName, const SearchOptions& options)
{
    static const QStringList builtInExclusions = {
        QStringLiteral(".git"), QStringLiteral(".svn"), QStringLiteral(".hg"),
        QStringLiteral("node_modules"), QStringLiteral(".idea"), QStringLiteral(".vs")
    };
    if (builtInExclusions.contains(directoryName, Qt::CaseInsensitive))
        return true;

    QStringList patterns = options.excludedPatterns;
    patterns.append(splitPatterns(options.excludePattern));
    return matchesAnyPattern(patterns, relativePath, directoryName);
}

QRegularExpression createExpression(const SearchOptions& options, QString* error)
{
    QString pattern = options.regularExpression
        ? options.query
        : QRegularExpression::escape(options.query);

    if (options.wholeWord) {
        pattern = QStringLiteral("(?<![\\p{L}\\p{N}_])(?:%1)(?![\\p{L}\\p{N}_])").arg(pattern);
    }

    QRegularExpression::PatternOptions patternOptions = QRegularExpression::UseUnicodePropertiesOption;
    if (!options.caseSensitive)
        patternOptions |= QRegularExpression::CaseInsensitiveOption;
    if (options.regularExpression)
        patternOptions |= QRegularExpression::MultilineOption;

    QRegularExpression expression(pattern, patternOptions);
    if (!expression.isValid() && error)
        *error = searchTr(QT_TRANSLATE_NOOP("FindEngine", "Invalid regular expression: %1"))
                     .arg(expression.errorString());
    return expression;
}

QString decodedUtf8(const QByteArray& contents, bool* ok)
{
    if (contents.left(8192).contains('\0')) {
        *ok = false;
        return {};
    }

    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder.decode(contents);
    *ok = !decoder.hasError();
    return text;
}

QVector<int> lineStarts(const QString& text)
{
    QVector<int> starts;
    starts.reserve(qMax(1, text.count('\n') + 1));
    starts.append(0);
    for (int index = 0; index < text.size(); ++index) {
        if (text.at(index) == QLatin1Char('\n'))
            starts.append(index + 1);
    }
    return starts;
}

QString replacementForMatch(const QRegularExpressionMatch& match, const QString& replacement)
{
    QString result;
    result.reserve(replacement.size());

    for (int index = 0; index < replacement.size(); ++index) {
        if (replacement.at(index) != QLatin1Char('\\') || index + 1 >= replacement.size()) {
            result.append(replacement.at(index));
            continue;
        }

        const QChar next = replacement.at(index + 1);
        if (next == QLatin1Char('\\')) {
            result.append(QLatin1Char('\\'));
            ++index;
            continue;
        }
        if (next == QLatin1Char('n')) {
            result.append(QLatin1Char('\n'));
            ++index;
            continue;
        }
        if (next == QLatin1Char('t')) {
            result.append(QLatin1Char('\t'));
            ++index;
            continue;
        }
        if (next.isDigit()) {
            int end = index + 1;
            while (end < replacement.size() && replacement.at(end).isDigit() && end - index <= 2)
                ++end;
            const int capture = replacement.mid(index + 1, end - index - 1).toInt();
            result.append(match.captured(capture));
            index = end - 1;
            continue;
        }

        result.append(next);
        ++index;
    }

    return result;
}

void appendMatches(const SearchDocument& document,
                   const SearchOptions& options,
                   const QRegularExpression& expression,
                   SearchReport* report)
{
    bool validUtf8 = false;
    const QString text = decodedUtf8(document.contents, &validUtf8);
    if (!validUtf8) {
        ++report->filesSkipped;
        return;
    }

    ++report->filesSearched;
    const QVector<int> starts = lineStarts(text);
    QRegularExpressionMatchIterator iterator = expression.globalMatch(text);
    while (iterator.hasNext()) {
        const QRegularExpressionMatch match = iterator.next();
        if (!match.hasMatch())
            continue;

        if (match.capturedLength() == 0) {
            report->error = searchTr(QT_TRANSLATE_NOOP("FindEngine", "The expression must not match empty text."));
            return;
        }

        const int offset = match.capturedStart();
        const auto upper = std::upper_bound(starts.cbegin(), starts.cend(), offset);
        const int lineIndex = qMax(0, static_cast<int>(std::distance(starts.cbegin(), upper)) - 1);
        const int start = starts.at(lineIndex);
        int end = text.indexOf(QLatin1Char('\n'), start);
        if (end < 0)
            end = text.size();

        QString preview = text.mid(start, end - start);
        if (preview.endsWith(QLatin1Char('\r')))
            preview.chop(1);
        preview = preview.trimmed();

        SearchMatch result;
        result.filePath = document.filePath;
        result.line = lineIndex + 1;
        result.column = offset - start;
        result.length = match.capturedLength();
        result.textOffset = offset;
        result.preview = preview;
        result.replacementText = options.regularExpression
            ? replacementForMatch(match, options.replacement)
            : options.replacement;
        report->matches.append(std::move(result));

        if (report->matches.size() >= options.maximumResults) {
            report->truncated = true;
            return;
        }
    }
}

} // namespace

SearchReport FindEngine::searchDocuments(const QVector<SearchDocument>& documents,
                                           const SearchOptions& options)
{
    SearchReport report;
    if (options.query.isEmpty())
        return report;

    const QRegularExpression expression = createExpression(options, &report.error);
    if (!expression.isValid())
        return report;

    for (const SearchDocument& document : documents) {
        appendMatches(document, options, expression, &report);
        if (!report.error.isEmpty() || report.truncated)
            break;
    }
    return report;
}

SearchReport FindEngine::searchProject(const QString& projectPath,
                                         const QVector<SearchDocument>& openDocuments,
                                         const SearchOptions& options)
{
    SearchReport report;
    if (options.query.isEmpty())
        return report;

    const QRegularExpression expression = createExpression(options, &report.error);
    if (!expression.isValid())
        return report;

    const QString root = normalizedPath(projectPath);
    QSet<QString> searchedPaths;
    for (const SearchDocument& document : openDocuments) {
        const QString path = normalizedPath(document.filePath);
        if (!path.startsWith(root + QLatin1Char('/'), Qt::CaseInsensitive) && path.compare(root, Qt::CaseInsensitive) != 0)
            continue;
        const QString relativePath = QDir(root).relativeFilePath(path).replace('\\', '/');
        if (!shouldIncludeFile(relativePath, options))
            continue;
        appendMatches(document, options, expression, &report);
        searchedPaths.insert(path);
        if (!report.error.isEmpty() || report.truncated)
            return report;
    }

    QVector<QString> directories{root};
    while (!directories.isEmpty()) {
        const QString directoryPath = directories.takeLast();
        const QDir directory(directoryPath);
        const QFileInfoList entries = directory.entryInfoList(
            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
            QDir::Name | QDir::DirsFirst);

        for (const QFileInfo& entry : entries) {
            const QString path = normalizedPath(entry.absoluteFilePath());
            const QString relativePath = QDir(root).relativeFilePath(path).replace('\\', '/');
            if (entry.isDir()) {
                if (!entry.isSymLink() && !shouldSkipDirectory(relativePath, entry.fileName(), options))
                    directories.append(path);
                continue;
            }

            if (!entry.isFile() || searchedPaths.contains(path) || !shouldIncludeFile(relativePath, options))
                continue;
            if (entry.size() > options.maximumFileSize) {
                ++report.filesSkipped;
                continue;
            }

            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                ++report.filesSkipped;
                continue;
            }
            appendMatches({path, file.readAll()}, options, expression, &report);
            if (!report.error.isEmpty() || report.truncated)
                return report;
        }
    }

    return report;
}

ReplacementResult FindEngine::replaceAll(const QByteArray& contents, const SearchOptions& options)
{
    ReplacementResult result;
    bool validUtf8 = false;
    QString text = decodedUtf8(contents, &validUtf8);
    if (!validUtf8) {
        result.error = searchTr(QT_TRANSLATE_NOOP("FindEngine", "The file is not valid UTF-8 text."));
        return result;
    }

    QString expressionError;
    const QRegularExpression expression = createExpression(options, &expressionError);
    if (!expression.isValid()) {
        result.error = expressionError;
        return result;
    }

    QRegularExpressionMatchIterator iterator = expression.globalMatch(text);
    QVector<QRegularExpressionMatch> matches;
    while (iterator.hasNext()) {
        const QRegularExpressionMatch match = iterator.next();
        if (match.capturedLength() == 0) {
            result.error = searchTr(QT_TRANSLATE_NOOP("FindEngine", "The expression must not match empty text."));
            return result;
        }
        matches.append(match);
    }

    for (auto it = matches.crbegin(); it != matches.crend(); ++it) {
        const QString replacement = options.regularExpression
            ? replacementForMatch(*it, options.replacement)
            : options.replacement;
        text.replace(it->capturedStart(), it->capturedLength(), replacement);
    }

    result.replacements = matches.size();
    result.contents = text.toUtf8();
    return result;
}

ReplacementResult FindEngine::replaceMatch(const QByteArray& contents,
                                             const SearchMatch& match,
                                             const SearchOptions& options)
{
    ReplacementResult result;
    bool validUtf8 = false;
    QString text = decodedUtf8(contents, &validUtf8);
    if (!validUtf8) {
        result.error = searchTr(QT_TRANSLATE_NOOP("FindEngine", "The file is not valid UTF-8 text."));
        return result;
    }

    QString expressionError;
    const QRegularExpression expression = createExpression(options, &expressionError);
    if (!expression.isValid()) {
        result.error = expressionError;
        return result;
    }

    const QRegularExpressionMatch current = expression.match(text, match.textOffset);
    if (!current.hasMatch() || current.capturedStart() != match.textOffset || current.capturedLength() != match.length) {
        result.error = searchTr(QT_TRANSLATE_NOOP("FindEngine", "The result changed. Search again before replacing it."));
        return result;
    }

    const QString replacement = options.regularExpression
        ? replacementForMatch(current, options.replacement)
        : options.replacement;
    text.replace(current.capturedStart(), current.capturedLength(), replacement);
    result.contents = text.toUtf8();
    result.replacements = 1;
    return result;
}
