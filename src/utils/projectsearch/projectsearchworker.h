#ifndef PROJECTSEARCHWORKER_H
#define PROJECTSEARCHWORKER_H

#include <QObject>
#include <atomic>

class ProjectSearchWorker : public QObject
{
    Q_OBJECT
public:
    explicit ProjectSearchWorker(QObject *parent = nullptr);

    void requestCancel();

public slots:
    void runSearch(QString rootPath, QString query);

signals:
    void hitsBatch(const QStringList &filePaths, const QList<int> &lineNumbers,
                   const QStringList &linePreviews);
    void searchFinished();

private:
    std::atomic_bool m_cancelled{false};
};

#endif
