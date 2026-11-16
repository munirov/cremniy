#pragma once

#include <QDateTime>
#include <QString>

/**
 * Presentation data for an inline line attribution in the code editor.
 * The editor owns this view model and has no dependency on its data source.
 */
struct EditorBlameLineInfo {
    QString authorName;
    QString authorEmail;
    QDateTime commitDate;
    QString shortOid;
    QString fullOid;
    QString commitSummary;
    bool isUncommitted = false;
};
