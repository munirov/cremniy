#include "filesyncdecision.h"

#include <QFileInfo>

FileSyncDecision::Action FileSyncDecision::decideFileChanged(bool hasUnsavedChanges)
{
    return hasUnsavedChanges ? Action::KeepCurrent : Action::Reload;
}

FileSyncDecision::Action FileSyncDecision::decideFileDisappeared(bool fileExists, bool hasUnsavedChanges)
{
    if (fileExists) {
        return Action::KeepCurrent;
    }

    return hasUnsavedChanges ? Action::CloseTab : Action::KeepCurrent;
}

QString FileSyncDecision::displayName(const QString& filePath)
{
    return QFileInfo(filePath).fileName();
}
