#ifndef GITMANAGER_H
#define GITMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <git2.h>

/**
 * @brief Класс-обёртка над libgit2 для всех гит операций
 * Все методы возвращают true при успехе и false при ошибке.
 * Текст ошибки можно получить через lastError().
 */
class GitManager : public QObject
{
    Q_OBJECT

public:
    explicit GitManager(QObject *parent = nullptr);
    ~GitManager() override;

    /** @brief Открыть репозиторий по пути */
    bool open(const QString &repoPath);

    /** @brief Закрыть текущий репозиторий */
    void close();

    /** @brief Проверить, открыт ли репозиторий */
    bool isOpen() const;

    /** @brief Получить последнюю ошибку */
    QString lastError() const;

    /** @brief Получить путь к репозиторию */
    QString repoPath() const;

    // Ветки

    /** @brief Получить список всех веток */
    QStringList branches() const;

    /** @brief Получить текущую ветку */
    QString currentBranch() const;

    /** @brief Переключиться на ветку */
    bool checkoutBranch(const QString &branchName);

    /** @brief Создать новую ветку */
    bool createBranch(const QString &branchName);

    /** @brief Удалить ветку */
    bool deleteBranch(const QString &branchName);

    /** @brief Переименовать ветку */
    bool renameBranch(const QString &oldName, const QString &newName);

    // Коммиты

    /** @brief Создать коммит */
    bool createCommit(const QString &message);

    /** @brief Получить историю коммитов (возвращает OID в виде hex строк) */
    QStringList commitHistory(int count = 50) const;

    /** @brief Получить сообщение коммита по OID */
    QString commitMessage(const QString &oid) const;

    /** @brief Получить автора коммита по OID */
    QString commitAuthor(const QString &oid) const;

    /** @brief Переключиться на коммит (detached HEAD) */
    bool checkoutCommit(const QString &oid);

    /** @brief Откатить коммит (reset --hard) */
    bool resetHard(const QString &oid);

    /** @brief Откатить коммит (reset --mixed) */
    bool resetMixed(const QString &oid);

    /** @brief Отменить коммит (revert) */
    bool revertCommit(const QString &oid);

    /** @brief Изменить последний коммит (amend) */
    bool amendCommit(const QString &message);

    // Синхронизация

    /** @brief Отправить изменения (push) */
    bool push(const QString &remote = "origin", const QString &branch = "");

    /** @brief Получить изменения (pull) */
    bool pull(const QString &remote = "origin", const QString &branch = "");

    /** @brief Получить изменения без слияния (fetch) */
    bool fetch(const QString &remote = "origin");

    // Слияние

    /** @brief Выполнить слияние ветки */
    bool merge(const QString &branchName);

    /** @brief Проверить, есть ли конфликты */
    bool hasConflicts() const;

    /** @brief Получить список конфликтных файлов */
    QStringList conflictFiles() const;

    // Индексация

    /** @brief Добавить файл в индекс */
    bool stageFile(const QString &filePath);

    /** @brief Удалить файл из индекса */
    bool unstageFile(const QString &filePath);

    /** @brief Получить diff для файла */
    QString fileDiff(const QString &filePath) const;

    /** @brief Получить diff для staged изменений */
    QString stagedDiff() const;

    // Репозиторий

    /** @brief Клонировать репозиторий */
    bool clone(const QString &url, const QString &path);

    /** @brief Инициализировать репозиторий */
    bool init(const QString &path);

    // Дополнительно

    /** @brief Получить статус репозитория */
    QString status() const;

    /** @brief Сохранить stash */
    bool stashSave(const QString &message = "");

    /** @brief Применить stash */
    bool stashApply(int index = 0);

    /** @brief Удалить stash */
    bool stashDrop(int index = 0);

    /** @brief Получить список stash */
    QStringList stashList() const;

    /** @brief Получить лог с графом веток (текстовое представление) */
    QString logGraph(int count = 50) const;

signals:
    /** @brief Сигнал об изменении репозитория */
    void repositoryChanged();

private:
    git_repository *m_repo = nullptr;
    QString m_repoPath;
    mutable QString m_lastError;

    /** @brief Установить сообщение об ошибке */
    void setError(const QString &error) const;

    /** @brief Получить имя пользователя из конфига */
    QString userName() const;

    /** @brief Получить email пользователя из конфига */
    QString userEmail() const;

    /** @brief Создать подпись (signature) */
    git_signature *createSignature() const;
};

#endif // GITMANAGER_H