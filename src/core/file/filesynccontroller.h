#ifndef FILESYNCCONTROLLER_H
#define FILESYNCCONTROLLER_H

#include <QObject>
#include <QHash>
#include <QString>

class FileSyncMonitor;

class FileSyncController : public QObject
{
    Q_OBJECT
public:
    explicit FileSyncController(QObject *parent = nullptr);

    void attachFile(const QString& filePath, bool hasUnsavedChanges = false);
    void detachFile(const QString& filePath);
    void updateFileState(const QString& filePath, bool hasUnsavedChanges);
    void refreshBaseline(const QString& filePath);

public slots:
    void handleFileChanged(const QString& filePath);
    void handleFileDisappeared(const QString& filePath);

signals:
    void reloadRequested(const QString& filePath);
    void keepCurrentRequested(const QString& filePath);
    void closeRequested(const QString& filePath);
    void markDirtyRequested(const QString& filePath);

private:
    FileSyncMonitor* m_monitor = nullptr;
    QHash<QString, bool> m_dirtyState;
};

#endif // FILESYNCCONTROLLER_H
