#pragma once

#include <QList>

enum class QHexChangeReason { None, Insert, Remove, Replace };

struct QHexChangeRange {
    QHexChangeReason reason;
    qint64 start;
    qint64 end;

    bool operator<(const QHexChangeRange& rhs) const {
        return start < rhs.start;
    }
};

using QHexChanges = QList<QHexChangeRange>;
