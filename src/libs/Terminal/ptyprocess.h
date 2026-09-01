#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcessEnvironment>
#include <QSize>
#include <QString>
#include <QStringList>

namespace Cremniy::Terminal {

struct PtyProcessOptions
{
    QString executable;
    QStringList arguments;
    QString workingDirectory;
    QProcessEnvironment environment;
    QSize initialSize{80, 24};
};

class PtyProcess : public QObject
{
    Q_OBJECT

public:
    explicit PtyProcess(QObject *parent = nullptr) : QObject(parent) {}
    ~PtyProcess() override = default;

    virtual bool start(const PtyProcessOptions &options) = 0;
    virtual qint64 write(const QByteArray &data) = 0;
    virtual bool resize(const QSize &size) = 0;
    virtual void stop() = 0;

    virtual bool isRunning() const = 0;
    virtual qint64 processId() const = 0;
    virtual QString lastError() const = 0;

signals:
    void dataReceived(const QByteArray &data);
    void started(qint64 processId);
    void finished(int exitCode);
    void errorOccurred(const QString &message);
};

PtyProcess *createPtyProcess(QObject *parent = nullptr);

} // namespace Cremniy::Terminal
