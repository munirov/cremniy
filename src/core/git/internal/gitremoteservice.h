#pragma once

#include <QString>

namespace GitInternal {

    class Repository;

    class RemoteService final {
    public:
        static bool push(Repository& repository,
                         const QString& remote,
                         const QString& branch);
        static bool pull(Repository& repository,
                         const QString& remote,
                         const QString& branch);
        static bool fetch(Repository& repository, const QString& remote);
    };

}// namespace GitInternal
