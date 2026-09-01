#pragma once

#include <QDateTime>
#include <QString>

/**
 * @brief Git metadata associated with a single line in a file.
 *
 * This transport type belongs to the Git core component. UI consumers may
 * render it in any way without making the Git implementation depend on them.
 */
struct BlameLineInfo {
    QString authorName;
    QString authorEmail;
    QDateTime commitDate;
    QString shortOid;
    QString fullOid;
    QString commitSummary;
    bool isUncommitted = false;
};
