#pragma once

#include <QString>
#include <QStringList>

namespace GitInternal {

    class Repository;

    class BranchService final {
    public:
        static QStringList branches(const Repository& repository);
        static QString currentBranch(const Repository& repository);
        static bool checkout(Repository& repository, const QString& branchName);
        static bool create(Repository& repository, const QString& branchName);
        static bool remove(Repository& repository, const QString& branchName);
        static bool rename(Repository& repository, const QString& oldName, const QString& newName);
    };

}// namespace GitInternal
