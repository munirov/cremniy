#include "projectsearchengine.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace ProjectSearchEngine {

namespace {

QSet<QString> skipDirNames()
{
    static const QSet<QString> k = {
        QStringLiteral(".git"),
        QStringLiteral(".svn"),
        QStringLiteral(".hg"),
        QStringLiteral(".vs"),
        QStringLiteral("node_modules"),
        QStringLiteral("__pycache__"),
        QStringLiteral(".idea"),
        QStringLiteral(".qtc_clangd"),
        QStringLiteral("cmake-build-debug"),
        QStringLiteral("cmake-build-release"),
        QStringLiteral("cmake-build-relwithdebinfo"),
        QStringLiteral("cmake-build-minsizerel"),
        QStringLiteral("build"),
        QStringLiteral("out"),
        QStringLiteral("dist"),
        QStringLiteral(".cache"),
    };
    return k;
}

} // namespace

bool isProbableBinarySample(const QByteArray &sample)
{
    return sample.contains('\0');
}

bool shouldSkipDirectoryName(const QString &name)
{
    return skipDirNames().contains(name.toLower());
}

bool pathContainsSkippedDirectory(const QString &rootPath,
                                  const QString &absoluteFilePath)
{
    const QString rootClean =
        QDir::cleanPath(QFileInfo(rootPath).absoluteFilePath());
    const QString fileClean =
        QDir::cleanPath(QFileInfo(absoluteFilePath).absoluteFilePath());

    if (!fileClean.startsWith(rootClean))
        return true;

    const QString rel = QDir::fromNativeSeparators(
        QDir(rootClean).relativeFilePath(fileClean));
    const QStringList parts = rel.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (shouldSkipDirectoryName(parts.at(i)))
            return true;
    }
    return false;
}

std::optional<QRegularExpression> buildLineMatcher(const ProjectSearchOptions &opt,
                                                   QString *errorMessage)
{
    QRegularExpression::PatternOptions po =
        opt.caseSensitive ? QRegularExpression::NoPatternOption
                          : QRegularExpression::CaseInsensitiveOption;

    QString pattern;
    if (opt.useRegex) {
        pattern = opt.query;
        if (opt.wholeWord)
            pattern = QStringLiteral(R"(\b(?:%1)\b)").arg(pattern);
    } else {
        const QString esc = QRegularExpression::escape(opt.query);
        pattern =
            opt.wholeWord ? QStringLiteral(R"(\b(?:%1)\b)").arg(esc) : esc;
    }

    QRegularExpression re(pattern, po);
    if (!re.isValid()) {
        if (errorMessage)
            *errorMessage = re.errorString();
        return std::nullopt;
    }
    return re;
}

bool lineMatches(const QString &line, const QRegularExpression &re)
{
    return re.match(line).hasMatch();
}

} // namespace ProjectSearchEngine
