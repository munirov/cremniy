#pragma once

#include <QString>

namespace GitInternal {

    class Repository;

    class MergeService final {
    public:
        static bool merge(Repository& repository, const QString& branchName);
    };

}// namespace GitInternal
