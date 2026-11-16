#pragma once

#include <QString>
#include <QStringList>

namespace GitInternal {

    class Repository;

    class IndexService final {
    public:
        static bool hasConflicts(const Repository& repository);
        static QStringList conflictFiles(const Repository& repository);
        static bool stage(Repository& repository, const QString& filePath);
        static bool unstage(Repository& repository, const QString& filePath);
        static QString fileDiff(const Repository& repository, const QString& filePath);
        static QString stagedDiff(const Repository& repository);
        static QString status(const Repository& repository, const QString& currentBranch);
    };

}// namespace GitInternal
