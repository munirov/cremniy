#include "ProjectSearchEngine.h"
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>

ProjectSearchEngine::ProjectSearchEngine(QObject* parent)
    : QObject(parent)
{
}

void ProjectSearchEngine::setSearchParams(const QString& projectPath,
                                          const QString& query,
                                          bool caseSensitive,
                                          bool useRegex,
                                          bool wholeWord,
                                          bool openFilesOnly,
                                          const QStringList& openFilePaths)
{
    m_projectPath = projectPath;
    m_query = query;
    m_caseSensitive = caseSensitive;
    m_useRegex = useRegex;
    m_wholeWord = wholeWord;
    m_openFilesOnly = openFilesOnly;
    m_openFilePaths = openFilePaths;
    m_stopFlag.storeRelaxed(0);
    m_totalMatches = 0;
    m_totalFiles = 0;
}

void ProjectSearchEngine::execute()
{
    m_stopFlag.storeRelaxed(0);
    m_totalMatches = 0;
    m_totalFiles = 0;

    QStringList files = collectFiles(m_projectPath, m_openFilesOnly);

    for (const QString& filePath : files) {
        if (m_stopFlag.loadRelaxed())
            break;

        searchFile(filePath);
    }

    emit searchFinished(m_totalMatches, m_totalFiles);
}

void ProjectSearchEngine::stop()
{
    m_stopFlag.storeRelaxed(1);
}

void ProjectSearchEngine::searchFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    Qt::CaseSensitivity cs = m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;

    QRegularExpression regex;
    if (m_useRegex) {
        QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
        if (!m_caseSensitive)
            opts |= QRegularExpression::CaseInsensitiveOption;
        regex.setPattern(m_query);
        regex.setPatternOptions(opts);

        if (!regex.isValid())
            return;
    }

    int lineNumber = 0;
    int fileMatchCount = 0;
    QTextStream stream(&file);

    while (!stream.atEnd()) {
        if (m_stopFlag.loadRelaxed())
            break;

        QString line = stream.readLine();
        lineNumber++;

        QList<QPair<int, int>> matches = findMatchesInLine(line, regex, m_query, m_wholeWord, cs);

        if (!matches.isEmpty()) {
            fileMatchCount += matches.size();

            for (const auto& match : matches) {
                SearchResult result;
                result.filePath = filePath;
                result.lineNumber = lineNumber;
                result.lineText = line;
                result.matchStart = match.first;
                result.matchLength = match.second;
                result.matchCountOnLine = matches.size();

                emit resultFound(result);
            }
        }
    }

    file.close();

    if (fileMatchCount > 0) {
        m_totalMatches += fileMatchCount;
        m_totalFiles++;
    }
}

QList<QPair<int, int>> ProjectSearchEngine::findMatchesInLine(const QString& line,
                                                               const QRegularExpression& regex,
                                                               const QString& plainQuery,
                                                               bool wholeWord,
                                                               Qt::CaseSensitivity cs)
{
    QList<QPair<int, int>> matches;

    if (m_useRegex) {
        QRegularExpressionMatchIterator it = regex.globalMatch(line);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            if (wholeWord) {
                int start = match.capturedStart();
                int end = match.capturedEnd();
                bool leftBound = (start == 0) || !line[start - 1].isLetterOrNumber();
                bool rightBound = (end == line.length()) || !line[end].isLetterOrNumber();
                if (!leftBound || !rightBound)
                    continue;
            }
            matches.append({match.capturedStart(), match.capturedLength()});
        }
    } else {
        int searchFrom = 0;
        while (true) {
            int index = line.indexOf(plainQuery, searchFrom, cs);
            if (index < 0)
                break;

            if (wholeWord) {
                bool leftBound = (index == 0) || !line[index - 1].isLetterOrNumber();
                int end = index + plainQuery.length();
                bool rightBound = (end == line.length()) || !line[end].isLetterOrNumber();
                if (!leftBound || !rightBound) {
                    searchFrom = index + 1;
                    continue;
                }
            }

            matches.append({index, plainQuery.length()});
            searchFrom = index + plainQuery.length();
        }
    }

    return matches;
}

QStringList ProjectSearchEngine::collectFiles(const QString& rootPath, bool openFilesOnly)
{
    QStringList files;

    if (openFilesOnly) {
        for (const QString& path : m_openFilePaths) {
            if (QFile::exists(path) && isTextFile(path))
                files.append(path);
        }
        return files;
    }

    static const QStringList skipDirs = {
        ".git", "build", "cmake-build-debug", "cmake-build-release",
        "cmake-build-relwithdebinfo", "cmake-build-minsizerel",
        "node_modules", ".venv", "__pycache__", ".mimocode", ".vscode"
    };

    QDirIterator it(rootPath, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        if (m_stopFlag.loadRelaxed())
            break;

        QString filePath = it.next();

        // Skip excluded directories
        bool skip = false;
        for (const QString& dir : skipDirs) {
            if (filePath.contains("/" + dir + "/") || filePath.contains("\\" + dir + "\\")) {
                skip = true;
                break;
            }
        }
        if (skip)
            continue;

        if (isTextFile(filePath))
            files.append(filePath);
    }

    return files;
}

bool ProjectSearchEngine::isTextFile(const QString& filePath) const
{
    QFileInfo fi(filePath);

    // Skip very large files (> 10MB)
    if (fi.size() > 10 * 1024 * 1024)
        return false;

    // Skip binary extensions
    static const QStringList binaryExts = {
        "exe", "dll", "so", "dylib", "o", "obj", "lib", "a",
        "bin", "dat", "db", "sqlite",
        "png", "jpg", "jpeg", "gif", "bmp", "ico", "svg",
        "mp3", "mp4", "avi", "mkv", "wav", "flac",
        "zip", "tar", "gz", "bz2", "xz", "7z", "rar",
        "pdf", "doc", "docx", "xls", "xlsx",
        "ttf", "otf", "woff", "woff2",
        "pyc", "pyo", "class", "jar"
    };

    QString ext = fi.suffix().toLower();
    if (binaryExts.contains(ext))
        return false;

    // Probe for null bytes
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray probe = file.read(4096);
    file.close();

    for (int i = 0; i < probe.size(); ++i) {
        if (probe[i] == '\0')
            return false;
    }

    return true;
}

void ProjectSearchEngine::replaceAllInProject(const QString& projectPath,
                                               const QString& query,
                                               const QString& replacement,
                                               bool caseSensitive,
                                               bool useRegex,
                                               bool wholeWord)
{
    Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    QStringList files = collectFiles(projectPath, false);

    for (const QString& filePath : files) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QString content = QString::fromUtf8(file.readAll());
        file.close();

        QString newContent;
        int searchFrom = 0;

        if (useRegex) {
            QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
            if (!caseSensitive)
                opts |= QRegularExpression::CaseInsensitiveOption;

            QRegularExpression regex(query, opts);
            if (!regex.isValid())
                continue;

            int lastEnd = 0;
            QRegularExpressionMatchIterator it = regex.globalMatch(content);
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                if (wholeWord) {
                    int start = match.capturedStart();
                    int end = match.capturedEnd();
                    bool leftBound = (start == 0) || !content[start - 1].isLetterOrNumber();
                    bool rightBound = (end == content.length()) || !content[end].isLetterOrNumber();
                    if (!leftBound || !rightBound)
                        continue;
                }
                newContent += content.mid(lastEnd, match.capturedStart() - lastEnd);
                newContent += replacement;
                lastEnd = match.capturedEnd();
            }
            newContent += content.mid(lastEnd);
        } else {
            int index = 0;
            while (true) {
                index = content.indexOf(query, searchFrom, cs);
                if (index < 0)
                    break;

                if (wholeWord) {
                    bool leftBound = (index == 0) || !content[index - 1].isLetterOrNumber();
                    int end = index + query.length();
                    bool rightBound = (end == content.length()) || !content[end].isLetterOrNumber();
                    if (!leftBound || !rightBound) {
                        searchFrom = index + 1;
                        continue;
                    }
                }

                content.replace(index, query.length(), replacement);
                searchFrom = index + replacement.length();
            }
            newContent = content;
        }

        if (newContent != content) {
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(newContent.toUtf8());
                file.close();
            }
        }
    }
}
