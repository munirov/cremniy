#include "gitmenu.h"
#include "ui/MenuBar/menufactory.h"
#include <QApplication>
#include <QFileDialog>
#include <QTextEdit>
#include <QDialog>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <memory>

// регистрируем меню
static bool registered = [](){
    MenuFactory::instance().registerMenu("7", [](){
        return std::make_unique<GitMenu>().release();
    });
    return true;
}();

GitMenu::GitMenu() : BaseMenu(tr("Git"))
{
    // Ветки
    m_branchMenu = addMenu(tr("Branches"));

    m_checkoutBranch = m_branchMenu->addAction(tr("Checkout Branch"));
    m_createBranch = m_branchMenu->addAction(tr("Create Branch"));
    m_deleteBranch = m_branchMenu->addAction(tr("Delete Branch"));
    m_renameBranch = m_branchMenu->addAction(tr("Rename Branch"));
    m_branchMenu->addSeparator();
    m_listBranches = m_branchMenu->addAction(tr("List Branches"));

    // Коммиты
    m_commitMenu = addMenu(tr("Commits"));

    m_createCommit = m_commitMenu->addAction(tr("Create Commit"));
    m_showHistory = m_commitMenu->addAction(tr("Commit History"));
    m_commitMenu->addSeparator();
    m_checkoutCommit = m_commitMenu->addAction(tr("Checkout Commit"));
    m_resetHard = m_commitMenu->addAction(tr("Reset (hard)"));
    m_resetMixed = m_commitMenu->addAction(tr("Reset (mixed)"));
    m_revertCommit = m_commitMenu->addAction(tr("Revert Commit"));
    m_commitMenu->addSeparator();
    m_amendCommit = m_commitMenu->addAction(tr("Amend Commit"));

    // Синхронизация
    m_syncMenu = addMenu(tr("Sync"));

    m_push = m_syncMenu->addAction(tr("Push"));
    m_pull = m_syncMenu->addAction(tr("Pull"));
    m_fetch = m_syncMenu->addAction(tr("Fetch"));

    // Слияние
    m_mergeMenu = addMenu(tr("Merge"));

    m_mergeBranch = m_mergeMenu->addAction(tr("Merge Branch"));
    m_showConflicts = m_mergeMenu->addAction(tr("Show Conflicts"));

    // Индексация
    m_stagingMenu = addMenu(tr("Staging"));

    m_stageFile = m_stagingMenu->addAction(tr("Stage File"));
    m_unstageFile = m_stagingMenu->addAction(tr("Unstage File"));
    m_stagingMenu->addSeparator();
    m_showDiff = m_stagingMenu->addAction(tr("Show Diff"));
    m_showStagedDiff = m_stagingMenu->addAction(tr("Show Staged Diff"));

    // Репозиторий
    m_repoMenu = addMenu(tr("Repository"));

    m_cloneRepo = m_repoMenu->addAction(tr("Clone Repository"));
    m_initRepo = m_repoMenu->addAction(tr("Init Repository"));
    m_openRepo = m_repoMenu->addAction(tr("Open Repository"));

    // Дополнительно
    m_extraMenu = addMenu(tr("Extra"));

    m_showStatus = m_extraMenu->addAction(tr("Status"));
    m_extraMenu->addSeparator();
    m_stashSave = m_extraMenu->addAction(tr("Save Stash"));
    m_stashApply = m_extraMenu->addAction(tr("Apply Stash"));
    m_stashDrop = m_extraMenu->addAction(tr("Delete Stash"));
    m_stashList = m_extraMenu->addAction(tr("Stash List"));
    m_extraMenu->addSeparator();
    m_showLogGraph = m_extraMenu->addAction(tr("Log Graph"));
}

void GitMenu::setupConnections(IDEWindow* ideWind)
{
    // создаём менеджер и запоминаем окно
    m_git = new GitManager(this);
    m_ideWind = ideWind;

    // автообнаружение репозитория у открытой папки проекта
    const QString projectPath = ideWind->property("projectPath").toString();
    const QString repoPath = findGitRepositoryRoot(projectPath);
    if (!repoPath.isEmpty()) {
        m_git->open(repoPath);
    }

    // проверяем путь раз в 2 секунды
    m_repoWatchTimer.setInterval(2000);
    connect(&m_repoWatchTimer, &QTimer::timeout, this, &GitMenu::onRepoWatchTimeout);
    m_repoWatchTimer.start();

    // Ветки
    connect(m_checkoutBranch, &QAction::triggered, this, &GitMenu::onCheckoutBranch);
    connect(m_createBranch, &QAction::triggered, this, &GitMenu::onCreateBranch);
    connect(m_deleteBranch, &QAction::triggered, this, &GitMenu::onDeleteBranch);
    connect(m_renameBranch, &QAction::triggered, this, &GitMenu::onRenameBranch);
    connect(m_listBranches, &QAction::triggered, this, &GitMenu::onListBranches);

    // Коммиты
    connect(m_createCommit, &QAction::triggered, this, &GitMenu::onCreateCommit);
    connect(m_showHistory, &QAction::triggered, this, &GitMenu::onShowHistory);
    connect(m_checkoutCommit, &QAction::triggered, this, &GitMenu::onCheckoutCommit);
    connect(m_resetHard, &QAction::triggered, this, &GitMenu::onResetHard);
    connect(m_resetMixed, &QAction::triggered, this, &GitMenu::onResetMixed);
    connect(m_revertCommit, &QAction::triggered, this, &GitMenu::onRevertCommit);
    connect(m_amendCommit, &QAction::triggered, this, &GitMenu::onAmendCommit);

    // Синхронизация
    connect(m_push, &QAction::triggered, this, &GitMenu::onPush);
    connect(m_pull, &QAction::triggered, this, &GitMenu::onPull);
    connect(m_fetch, &QAction::triggered, this, &GitMenu::onFetch);

    // Слияние
    connect(m_mergeBranch, &QAction::triggered, this, &GitMenu::onMergeBranch);
    connect(m_showConflicts, &QAction::triggered, this, &GitMenu::onShowConflicts);

    // Индексация
    connect(m_stageFile, &QAction::triggered, this, &GitMenu::onStageFile);
    connect(m_unstageFile, &QAction::triggered, this, &GitMenu::onUnstageFile);
    connect(m_showDiff, &QAction::triggered, this, &GitMenu::onShowDiff);
    connect(m_showStagedDiff, &QAction::triggered, this, &GitMenu::onShowStagedDiff);

    // Репозиторий
    connect(m_cloneRepo, &QAction::triggered, this, &GitMenu::onCloneRepo);
    connect(m_initRepo, &QAction::triggered, this, &GitMenu::onInitRepo);
    connect(m_openRepo, &QAction::triggered, this, &GitMenu::onOpenRepo);

    // Дополнительно
    connect(m_showStatus, &QAction::triggered, this, &GitMenu::onShowStatus);
    connect(m_stashSave, &QAction::triggered, this, &GitMenu::onStashSave);
    connect(m_stashApply, &QAction::triggered, this, &GitMenu::onStashApply);
    connect(m_stashDrop, &QAction::triggered, this, &GitMenu::onStashDrop);
    connect(m_stashList, &QAction::triggered, this, &GitMenu::onStashList);
    connect(m_showLogGraph, &QAction::triggered, this, &GitMenu::onShowLogGraph);
}

// Вспомогательные методы

void GitMenu::showError(const QString &title, const QString &message)
{
    QMessageBox::warning(nullptr, title, message);
}

void GitMenu::showInfo(const QString &title, const QString &message)
{
    QMessageBox::information(nullptr, title, message);
}

QString GitMenu::inputDialog(const QString &title, const QString &label)
{
    bool ok = false;
    QString result = QInputDialog::getText(nullptr, title, label, QLineEdit::Normal, "", &ok);
    return ok ? result : QString();
}

// Ветки

void GitMenu::onCheckoutBranch()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString branch = inputDialog(tr("Checkout Branch"), tr("Enter branch name:"));
    if (branch.isEmpty()) return;

    if (m_git->checkoutBranch(branch)) {
        showInfo(tr("Success"), tr("Переключились на ветку: ") + branch);
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onCreateBranch()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString branch = inputDialog(tr("Create Branch"), tr("Enter new branch name:"));
    if (branch.isEmpty()) return;

    if (m_git->createBranch(branch)) {
        showInfo(tr("Success"), tr("Branch created: ") + branch);
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onDeleteBranch()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString branch = inputDialog(tr("Delete Branch"), tr("Enter branch name to delete:"));
    if (branch.isEmpty()) return;

    if (m_git->deleteBranch(branch)) {
        showInfo(tr("Success"), tr("Удалена ветка: ") + branch);
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onRenameBranch()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString oldName = inputDialog(tr("Rename Branch"), tr("Current branch name:"));
    if (oldName.isEmpty()) return;

    const QString newName = inputDialog(tr("Rename Branch"), tr("New branch name:"));
    if (newName.isEmpty()) return;

    if (m_git->renameBranch(oldName, newName)) {
        showInfo(tr("Success"), tr("Ветка переименована: ") + oldName + " -> " + newName);
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onListBranches()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QStringList branches = m_git->branches();
    const QString current = m_git->currentBranch();

    QString text;
    for (const QString &b : branches) {
        if (b == current) {
            text += "* " + b + " (current)\n";
        } else {
            text += "  " + b + "\n";
        }
    }

    showInfo(tr("Branches"), text);
}

// Коммиты

void GitMenu::onCreateCommit()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString message = inputDialog(tr("Create Commit"), tr("Enter commit message:"));
    if (message.isEmpty()) return;

    if (m_git->createCommit(message)) {
        showInfo(tr("Success"), tr("Commit created"));
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onShowHistory()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QStringList history = m_git->commitHistory(30);
    if (history.isEmpty()) {
        showInfo(tr("History"), tr("No commits"));
        return;
    }

    QString text;
    for (const QString &oid : history) {
        const QString msg = m_git->commitMessage(oid);
        const QString author = m_git->commitAuthor(oid);
        text += oid.left(7) + " | " + author + " | " + msg.left(60) + "\n";
    }

    // показываем в диалоге
    QDialog dlg;
    dlg.setWindowTitle(tr("Commit History"));
    dlg.resize(700, 500);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    QTextEdit *textEdit = new QTextEdit(&dlg);
    textEdit->setPlainText(text);
    textEdit->setReadOnly(true);
    layout->addWidget(textEdit);

    QPushButton *closeBtn = new QPushButton(tr("Close"), &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(closeBtn);

    dlg.exec();
}

void GitMenu::onCheckoutCommit()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString oid = inputDialog(tr("Checkout Commit"), tr("Enter commit hash:"));
    if (oid.isEmpty()) return;

    if (m_git->checkoutCommit(oid)) {
        showInfo(tr("Success"), tr("Переключились на коммит: ") + oid);
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onRepoWatchTimeout()
{
    if (!m_ideWind) return;

    const QString projectPath = m_ideWind->property("projectPath").toString();
    if (projectPath.isEmpty()) return;

    const bool currentlyOpen = m_git->isOpen();
    const QString repoPath = findGitRepositoryRoot(projectPath);

    if (!currentlyOpen && !repoPath.isEmpty()) {
        m_git->open(repoPath);
    } else if (currentlyOpen && repoPath.isEmpty()) {
        m_git->close();
    }
}

void GitMenu::onResetHard()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const auto reply = QMessageBox::question(nullptr, tr("Reset (hard)"),
        tr("This will delete all uncommitted changes. Continue?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    const QString oid = inputDialog(tr("Reset (hard)"), tr("Enter commit hash to reset:"));
    if (oid.isEmpty()) return;

    if (m_git->resetHard(oid)) {
        showInfo(tr("Success"), tr("Reset completed"));
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onResetMixed()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString oid = inputDialog(tr("Reset (mixed)"), tr("Enter commit hash to reset:"));
    if (oid.isEmpty()) return;

    if (m_git->resetMixed(oid)) {
        showInfo(tr("Success"), tr("Reset completed"));
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onRevertCommit()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString oid = inputDialog(tr("Revert Commit"), tr("Enter commit hash to revert:"));
    if (oid.isEmpty()) return;

    if (m_git->revertCommit(oid)) {
        showInfo(tr("Success"), tr("Commit reverted"));
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onAmendCommit()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString message = inputDialog(tr("Amend Commit"), tr("New message:"));
    if (message.isEmpty()) return;

    if (m_git->amendCommit(message)) {
        showInfo(tr("Success"), tr("Last commit amended"));
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

// Синхронизация

void GitMenu::onPush()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    if (m_git->push()) {
        showInfo(tr("Success"), tr("Changes pushed"));
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onPull()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    if (m_git->pull()) {
        showInfo(tr("Success"), tr("Changes pulled"));
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onFetch()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    if (m_git->fetch()) {
        showInfo(tr("Success"), tr("Changes fetched"));
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

// Слияние

void GitMenu::onMergeBranch()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString branch = inputDialog(tr("Merge"), tr("Enter branch name to merge:"));
    if (branch.isEmpty()) return;

    if (m_git->merge(branch)) {
        if (m_git->hasConflicts()) {
            showError(tr("Conflicts"), tr("Conflicts detected. Resolve them and make a commit."));
        } else {
            showInfo(tr("Success"), tr("Merge completed"));
        }
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onShowConflicts()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QStringList conflicts = m_git->conflictFiles();
    if (conflicts.isEmpty()) {
        showInfo(tr("Conflicts"), tr("No conflicts"));
    } else {
        showInfo(tr("Conflict files"), conflicts.join("\n"));
    }
}

// Индексация

void GitMenu::onStageFile()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString file = inputDialog(tr("Stage File"), tr("Enter file path (relative to repo root):"));
    if (file.isEmpty()) return;

    if (m_git->stageFile(file)) {
        showInfo(tr("Success"), tr("Файл добавлен в индекс: ") + file);
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onUnstageFile()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString file = inputDialog(tr("Unstage File"), tr("Enter file path:"));
    if (file.isEmpty()) return;

    if (m_git->unstageFile(file)) {
        showInfo(tr("Success"), tr("Файл убран из индекса: ") + file);
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onShowDiff()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString file = inputDialog(tr("Show Diff"), tr("Enter file path:"));
    if (file.isEmpty()) return;

    const QString diff = m_git->fileDiff(file);
    if (diff.isEmpty()) {
        showInfo(tr("Diff"), tr("No changes"));
        return;
    }

    QDialog dlg;
    dlg.setWindowTitle(tr("Diff: ") + file);
    dlg.resize(700, 500);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    QTextEdit *textEdit = new QTextEdit(&dlg);
    textEdit->setPlainText(diff);
    textEdit->setReadOnly(true);
    layout->addWidget(textEdit);

    QPushButton *closeBtn = new QPushButton(tr("Close"), &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(closeBtn);

    dlg.exec();
}

void GitMenu::onShowStagedDiff()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString diff = m_git->stagedDiff();
    if (diff.isEmpty()) {
        showInfo(tr("Staged Diff"), tr("No staged changes"));
        return;
    }

    QDialog dlg;
    dlg.setWindowTitle(tr("Staged Diff"));
    dlg.resize(700, 500);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    QTextEdit *textEdit = new QTextEdit(&dlg);
    textEdit->setPlainText(diff);
    textEdit->setReadOnly(true);
    layout->addWidget(textEdit);

    QPushButton *closeBtn = new QPushButton(tr("Close"), &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(closeBtn);

    dlg.exec();
}

// Репозиторий

void GitMenu::onCloneRepo()
{
    const QString url = inputDialog(tr("Clone Repository"), tr("Repository URL:"));
    if (url.isEmpty()) return;

    const QString path = QFileDialog::getExistingDirectory(nullptr, tr("Select folder to clone into"));
    if (path.isEmpty()) return;

    if (m_git->clone(url, path)) {
        showInfo(tr("Success"), tr("Репозиторий клонирован в: ") + path);
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onInitRepo()
{
    const QString path = QFileDialog::getExistingDirectory(nullptr, tr("Select folder to initialize"));
    if (path.isEmpty()) return;

    if (m_git->init(path)) {
        showInfo(tr("Success"), tr("Репозиторий инициализирован в: ") + path);
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onOpenRepo()
{
    // пользователь сам выбирает папку с репозиторием
    const QString path = QFileDialog::getExistingDirectory(nullptr, tr("Select repository folder"));
    if (path.isEmpty()) return;

    if (m_git->open(path)) {
        showInfo(tr("Success"), tr("Репозиторий открыт: ") + path);
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

// Дополнительно

void GitMenu::onShowStatus()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString status = m_git->status();
    if (status.isEmpty()) {
        showInfo(tr("Status"), tr("No changes"));
        return;
    }

    QDialog dlg;
    dlg.setWindowTitle(tr("Repository status"));
    dlg.resize(600, 400);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    QTextEdit *textEdit = new QTextEdit(&dlg);
    textEdit->setPlainText(status);
    textEdit->setReadOnly(true);
    layout->addWidget(textEdit);

    QPushButton *closeBtn = new QPushButton(tr("Close"), &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(closeBtn);

    dlg.exec();
}

void GitMenu::onStashSave()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString message = inputDialog(tr("Save Stash"), tr("Message (optional):"));

    if (m_git->stashSave(message)) {
        showInfo(tr("Success"), tr("Changes saved to stash"));
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onStashApply()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString indexStr = inputDialog(tr("Apply Stash"), tr("Stash index (0 - latest):"));
    const int index = indexStr.toInt();

    if (m_git->stashApply(index)) {
        showInfo(tr("Success"), tr("Stash applied"));
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onStashDrop()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString indexStr = inputDialog(tr("Delete Stash"), tr("Stash index (0 - latest):"));
    const int index = indexStr.toInt();

    if (m_git->stashDrop(index)) {
        showInfo(tr("Success"), tr("Stash deleted"));
    } else {
        showError(tr("Error"), m_git->lastError());
    }
}

void GitMenu::onStashList()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QStringList stashes = m_git->stashList();
    if (stashes.isEmpty()) {
        showInfo(tr("Stash list"), tr("No stash saved"));
        return;
    }

    QString text;
    for (int i = 0; i < stashes.size(); i++) {
        text += QString::number(i) + ": " + stashes[i] + "\n";
    }

    showInfo(tr("Stash list"), text);
}

void GitMenu::onShowLogGraph()
{
    if (!m_git || !m_git->isOpen()) {
        showError(tr("Error"), tr("Repository not open"));
        return;
    }

    const QString log = m_git->logGraph(50);
    if (log.isEmpty()) {
        showInfo(tr("Log"), tr("No commits"));
        return;
    }

    QDialog dlg;
    dlg.setWindowTitle(tr("Log Graph"));
    dlg.resize(800, 600);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    QTextEdit *textEdit = new QTextEdit(&dlg);
    textEdit->setPlainText(log);
    textEdit->setReadOnly(true);
    textEdit->setFont(QFont("monospace", 10));
    layout->addWidget(textEdit);

    QPushButton *closeBtn = new QPushButton(tr("Close"), &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(closeBtn);

    dlg.exec();
}

QString GitMenu::findGitRepositoryRoot(const QString &path)
{
    QDir dir(path);
    
    // Ищем гит директорию, поднимаясь по иерархии
    while (!dir.isRoot()) {
        if (dir.exists(".git")) {
            QDir gitDir(dir.filePath(".git"));
            // Проверяем, что это директория (а не файл, как в случае worktrees)
            if (gitDir.exists() || QFileInfo(dir.filePath(".git")).isDir()) {
                return dir.absolutePath();
            }
        }
        if (!dir.cdUp()) break;
    }
    
    return {};
}

bool GitMenu::isGitRepository(const QString &path)
{
    return !findGitRepositoryRoot(path).isEmpty();
}
