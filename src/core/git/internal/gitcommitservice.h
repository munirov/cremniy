#pragma once

#include <QString>
#include <QStringList>

namespace GitInternal {

    class Repository;

    class CommitService final {
    public:
        static bool create(Repository& repository, const QString& message);
        static QStringList history(const Repository& repository, int count);
        static QString message(const Repository& repository, const QString& oid);
        static QString author(const Repository& repository, const QString& oid);
        static bool checkout(Repository& repository, const QString& oid);
        static bool resetHard(Repository& repository, const QString& oid);
        static bool resetMixed(Repository& repository, const QString& oid);
        static bool revert(Repository& repository, const QString& oid);
        static bool amend(Repository& repository, const QString& message);
        static QString graph(const Repository& repository, int count);

    private:
        static bool reset(Repository& repository, const QString& oid, int resetType);
    };

}// namespace GitInternal
