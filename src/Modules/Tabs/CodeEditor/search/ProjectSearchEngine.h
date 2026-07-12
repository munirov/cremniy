#ifndef PROJECTSEARCHENGINE_H
#define PROJECTSEARCHENGINE_H

#include <QObject>
#include <QAtomicInt>
#include <QStringList>
#include <QRegularExpression>
#include "SearchDefs.h"

class ProjectSearchEngine : public QObject
{
    Q_OBJECT

public:
    explicit ProjectSearchEngine(QObject* parent = nullptr);

    void setSearchParams(const QString& projectPath,
                         const QString& query,
                         bool caseSensitive,
                         bool useRegex,
                         bool wholeWord,
                         bool openFilesOnly,
                         const QStringList& openFilePaths);

    void replaceAllInProject(const QString& projectPath,
                             const QString& query,
                             const QString& replacement,
                             bool caseSensitive,
                             bool useRegex,
                             bool wholeWord);

public slots:
    void execute();
    void stop();

signals:
    void resultFound(const SearchResult& result);
    void searchFinished(int totalMatches, int totalFiles);

private:
    void searchFile(const QString& filePath);
    QList<QPair<int, int>> findMatchesInLine(const QString& line,
                                              const QRegularExpression& regex,
                                              const QString& plainQuery,
                                              bool wholeWord,
                                              Qt::CaseSensitivity cs);
    QStringList collectFiles(const QString& rootPath, bool openFilesOnly);
    bool isTextFile(const QString& filePath) const;

    QString m_projectPath;
    QString m_query;
    bool m_caseSensitive = false;
    bool m_useRegex = false;
    bool m_wholeWord = false;
    bool m_openFilesOnly = false;
    QStringList m_openFilePaths;
    QAtomicInt m_stopFlag;
    int m_totalMatches = 0;
    int m_totalFiles = 0;
};

#endif // PROJECTSEARCHENGINE_H
