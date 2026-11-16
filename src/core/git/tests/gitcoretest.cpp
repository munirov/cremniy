#include "gitmanager.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {

    bool writeFile(const QString& path, const QByteArray& contents) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
               && file.write(contents) == contents.size();
    }

}// namespace

class GitCoreTest : public QObject {
    Q_OBJECT

private slots:
    void discoversRepositoryFromNestedDirectory();
    void discoversWorktreeMarkerFile();
    void returnsEmptyOutsideRepository();
    void ownsRepositoryLifecycle();
    void reportsCommitValidationErrors();
    void stagesFilesAndReportsStatus();
    void filtersDiffByFile();
};

void GitCoreTest::discoversRepositoryFromNestedDirectory() {
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QDir root(temporaryDirectory.path());
    QVERIFY(root.mkpath(QStringLiteral("repo/.git")));
    QVERIFY(root.mkpath(QStringLiteral("repo/src/nested")));

    const QString repoPath = root.absoluteFilePath(QStringLiteral("repo"));
    const QString nestedPath = root.absoluteFilePath(QStringLiteral("repo/src/nested"));
    QCOMPARE(GitManager::findGitRepositoryRoot(nestedPath), QDir(repoPath).absolutePath());
}

void GitCoreTest::discoversWorktreeMarkerFile() {
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QDir root(temporaryDirectory.path());
    QVERIFY(root.mkpath(QStringLiteral("worktree/src")));

    QFile marker(root.absoluteFilePath(QStringLiteral("worktree/.git")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    QVERIFY(marker.write("gitdir: ../repository/.git/worktrees/worktree\n") > 0);
    marker.close();

    const QString repoPath = root.absoluteFilePath(QStringLiteral("worktree"));
    const QString sourcePath = root.absoluteFilePath(QStringLiteral("worktree/src"));
    QCOMPARE(GitManager::findGitRepositoryRoot(sourcePath), QDir(repoPath).absolutePath());
}

void GitCoreTest::returnsEmptyOutsideRepository() {
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QVERIFY(GitManager::findGitRepositoryRoot(temporaryDirectory.path()).isEmpty());
}

void GitCoreTest::ownsRepositoryLifecycle() {
    QTemporaryDir temporaryDirectory(QDir::current().absoluteFilePath(QStringLiteral("gitcoretest-XXXXXX")));
    QVERIFY(temporaryDirectory.isValid());

    const QString repositoryPath = QDir(temporaryDirectory.path()).absoluteFilePath(QStringLiteral("repository"));
    QVERIFY(QDir().mkpath(repositoryPath));
    GitManager git;
    QVERIFY2(git.init(repositoryPath), qPrintable(git.lastError()));
    QVERIFY(git.isOpen());
    QCOMPARE(QDir(git.repoPath()).absolutePath(), QDir(repositoryPath).absolutePath());

    git.close();
    QVERIFY(!git.isOpen());
    QVERIFY(git.repoPath().isEmpty());
}

void GitCoreTest::reportsCommitValidationErrors() {
    GitManager git;
    QVERIFY(!git.checkoutCommit(QStringLiteral("not-an-oid")));
    QCOMPARE(git.lastError(), QStringLiteral("Repository not open"));
}

void GitCoreTest::stagesFilesAndReportsStatus() {
    QTemporaryDir temporaryDirectory(QDir::current().absoluteFilePath(QStringLiteral("gitcoretest-XXXXXX")));
    QVERIFY(temporaryDirectory.isValid());

    GitManager git;
    QVERIFY2(git.init(temporaryDirectory.path()), qPrintable(git.lastError()));
    QVERIFY(writeFile(QDir(temporaryDirectory.path()).filePath(QStringLiteral("tracked.txt")), "contents\n"));

    QSignalSpy changedSpy(&git, &GitManager::repositoryChanged);
    QVERIFY2(git.stageFile(QStringLiteral("tracked.txt")), qPrintable(git.lastError()));
    QCOMPARE(changedSpy.count(), 1);
    QVERIFY(git.status().contains(QStringLiteral("tracked.txt")));

    QVERIFY2(git.unstageFile(QStringLiteral("tracked.txt")), qPrintable(git.lastError()));
    QCOMPARE(changedSpy.count(), 2);
    QVERIFY(git.status().contains(QStringLiteral("untracked:")));
}

void GitCoreTest::filtersDiffByFile() {
    QTemporaryDir temporaryDirectory(QDir::current().absoluteFilePath(QStringLiteral("gitcoretest-XXXXXX")));
    QVERIFY(temporaryDirectory.isValid());

    const QDir directory(temporaryDirectory.path());
    const QString firstPath = directory.filePath(QStringLiteral("first.txt"));
    const QString secondPath = directory.filePath(QStringLiteral("second.txt"));

    GitManager git;
    QVERIFY2(git.init(temporaryDirectory.path()), qPrintable(git.lastError()));
    QVERIFY(writeFile(firstPath, "first\n"));
    QVERIFY(writeFile(secondPath, "second\n"));
    QVERIFY2(git.stageFile(QStringLiteral("first.txt")), qPrintable(git.lastError()));
    QVERIFY2(git.stageFile(QStringLiteral("second.txt")), qPrintable(git.lastError()));
    QVERIFY2(git.createCommit(QStringLiteral("initial")), qPrintable(git.lastError()));

    QCOMPARE(git.commitHistory(1).size(), 1);
    QVERIFY(git.logGraph(1).contains(QStringLiteral("initial")));

    QVERIFY(writeFile(firstPath, "first changed\n"));
    QVERIFY(writeFile(secondPath, "second changed\n"));
    const QString diff = git.fileDiff(QStringLiteral("first.txt"));
    QVERIFY(diff.contains(QStringLiteral("first.txt")));
    QVERIFY(!diff.contains(QStringLiteral("second.txt")));
}

QTEST_GUILESS_MAIN(GitCoreTest)
#include "gitcoretest.moc"
