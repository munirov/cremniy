#include "filesynccontroller.h"

#include <QFileInfo>

#include "filesyncdecision.h"
#include "filesyncmonitor.h"

FileSyncController::FileSyncController(QObject *parent)
    : QObject(parent)
    , m_monitor(new FileSyncMonitor(this))
{
    connect(m_monitor, &FileSyncMonitor::fileChangedOnDisk,
            this, &FileSyncController::handleFileChanged);
    connect(m_monitor, &FileSyncMonitor::fileDisappeared,
            this, &FileSyncController::handleFileDisappeared);
}

void FileSyncController::attachFile(const QString& filePath, bool hasUnsavedChanges)
{
    if (filePath.isEmpty())
        return;

    m_dirtyState.insert(filePath, hasUnsavedChanges);
    m_monitor->watch(filePath);
}

void FileSyncController::detachFile(const QString& filePath)
{
    if (filePath.isEmpty())
        return;

    m_dirtyState.remove(filePath);
    m_monitor->unwatch(filePath);
}

void FileSyncController::updateFileState(const QString& filePath, bool hasUnsavedChanges)
{
    if (!filePath.isEmpty()) {
        m_dirtyState.insert(filePath, hasUnsavedChanges);
    }
}

void FileSyncController::refreshBaseline(const QString& filePath)
{
    m_monitor->refreshBaseline(filePath);
}

void FileSyncController::handleFileChanged(const QString& filePath)
{
    if (!QFileInfo::exists(filePath))
        return;

    const bool hasUnsavedChanges = m_dirtyState.value(filePath, false);
    const auto action = FileSyncDecision::decideFileChanged(hasUnsavedChanges);

    if (action == FileSyncDecision::Action::Reload) {
        emit reloadRequested(filePath);
    } else {
        emit keepCurrentRequested(filePath);
    }
}

void FileSyncController::handleFileDisappeared(const QString& filePath)
{
    if (QFileInfo::exists(filePath)) {
        refreshBaseline(filePath);
        return;
    }

    const bool hasUnsavedChanges = m_dirtyState.value(filePath, false);
    const auto action = FileSyncDecision::decideFileDisappeared(false, hasUnsavedChanges);

    if (action == FileSyncDecision::Action::CloseTab) {
        emit closeRequested(filePath);
    } else {
        emit markDirtyRequested(filePath);
    }
}
