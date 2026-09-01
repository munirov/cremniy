#include "ptyprocess.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

using namespace Cremniy::Terminal;

class PtyProcessTest final : public QObject
{
    Q_OBJECT

private slots:
    void startsShellAndTransfersOutput();
};

void PtyProcessTest::startsShellAndTransfersOutput()
{
    QScopedPointer<PtyProcess> process(createPtyProcess());
    QByteArray output;
    QString error;
    connect(process.get(), &PtyProcess::dataReceived, this, [&output](const QByteArray &data) {
        output += data;
    });
    connect(process.get(), &PtyProcess::errorOccurred, this, [&error](const QString &message) {
        error = message;
    });

    PtyProcessOptions options;
#ifdef Q_OS_WIN
    options.executable = QStandardPaths::findExecutable(QStringLiteral("cmd.exe"));
    options.arguments = {QStringLiteral("/D"),
                         QStringLiteral("/Q"),
                         QStringLiteral("/K"),
                         QStringLiteral("echo CREMNIY_PTY_READY")};
#else
    options.executable = QStringLiteral("/bin/sh");
    options.arguments = {QStringLiteral("-c"),
                         QStringLiteral("printf CREMNIY_PTY_READY; exec /bin/sh")};
#endif
    options.workingDirectory = QDir::tempPath();
    options.environment = QProcessEnvironment::systemEnvironment();
    options.environment.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    options.initialSize = QSize(80, 24);

    QVERIFY2(!options.executable.isEmpty(), "No shell executable was found");
    QVERIFY2(process->start(options), qPrintable(process->lastError()));
    QTRY_VERIFY_WITH_TIMEOUT(output.contains("CREMNIY_PTY_READY"), 5000);
#ifdef Q_OS_WIN
    const QByteArray command = QByteArrayLiteral("echo CREMNIY_PTY_OK\r\n");
#else
    const QByteArray command = QByteArrayLiteral("printf CREMNIY_PTY_OK\r\n");
#endif
    QCOMPARE(process->write(command), command.size());
    QTRY_VERIFY_WITH_TIMEOUT(output.contains("CREMNIY_PTY_OK"), 5000);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    process->stop();
}

QTEST_GUILESS_MAIN(PtyProcessTest)

#include "ptyprocesstest.moc"
