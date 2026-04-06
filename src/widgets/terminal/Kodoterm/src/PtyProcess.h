// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <QObject>
#include <QProcessEnvironment>
#include <QSize>
#include <QStringList>

class PtyProcess : public QObject {
    Q_OBJECT

  public:
    explicit PtyProcess(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~PtyProcess() = default;

    void setProgram(const QString &program) { m_program = program; }
    QString program() const { return m_program; }

    void setArguments(const QStringList &arguments) { m_arguments = arguments; }
    QStringList arguments() const { return m_arguments; }

    void setWorkingDirectory(const QString &workingDirectory) {
        m_workingDirectory = workingDirectory;
    }
    QString workingDirectory() const { return m_workingDirectory; }

    void setProcessEnvironment(const QProcessEnvironment &environment) {
        m_environment = environment;
    }
    QProcessEnvironment processEnvironment() const { return m_environment; }

    virtual bool start(const QSize &size) = 0;
    virtual bool start(const QString &program, const QStringList &arguments, const QSize &size);
    virtual void write(const QByteArray &data) = 0;
    virtual void resize(const QSize &size) = 0;
    virtual void kill() = 0;
    virtual bool isRoot() const = 0;
    virtual QString foregroundProcessName() const = 0;

    // Factory method
    static PtyProcess *create(QObject *parent = nullptr);

  signals:
    void readyRead(const QByteArray &data);
    void finished(int exitCode, int exitStatus);

  protected:
    QString m_program;
    QStringList m_arguments;
    QString m_workingDirectory;
    QProcessEnvironment m_environment = QProcessEnvironment::systemEnvironment();
};
