#pragma once

#include <QString>
#include <QStringList>

namespace GitInternal {

    class Repository;

    class StashService final {
    public:
        static bool save(Repository& repository, const QString& message);
        static bool apply(Repository& repository, int index);
        static bool drop(Repository& repository, int index);
        static QStringList list(const Repository& repository);
    };

}// namespace GitInternal
