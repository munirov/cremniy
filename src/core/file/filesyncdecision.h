#ifndef FILESYNCDECISION_H
#define FILESYNCDECISION_H

#include <QString>

class FileSyncDecision
{
public:
    enum class Action {
        Reload,
        KeepCurrent,
        CloseTab
    };

    static Action decideFileChanged(bool hasUnsavedChanges);
    static Action decideFileDisappeared(bool fileExists, bool hasUnsavedChanges);
    static QString displayName(const QString& filePath);
};

#endif // FILESYNCDECISION_H
