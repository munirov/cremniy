#pragma once

#include "blamelineinfo.h"

#include <QString>
#include <QVector>

namespace GitInternal {

    class Repository;

    /** Computes synchronous blame data for an opened repository. */
    class BlameEngine final {
    public:
        static QVector<BlameLineInfo> blameFile(Repository& repository,
                                                const QString& relativeFilePath);
    };

}// namespace GitInternal
