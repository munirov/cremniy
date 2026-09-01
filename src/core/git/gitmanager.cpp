#include "gitmanager.h"
#include "internal/gitblameengine.h"
#include "internal/gitbranchservice.h"
#include "internal/gitcommitservice.h"
#include "internal/gitindexservice.h"
#include "internal/gitmergeservice.h"
#include "internal/gitremoteservice.h"
#include "internal/gitrepository.h"
#include "internal/gitstashservice.h"

GitManager::GitManager(QObject* parent)
    : QObject(parent), m_repository(std::make_unique<GitInternal::Repository>()) {
}

GitManager::~GitManager() = default;

bool GitManager::open(const QString& repoPath) {
    return m_repository->open(repoPath);
}

void GitManager::close() {
    m_repository->close();
}

bool GitManager::isOpen() const {
    return m_repository->isOpen();
}

QString GitManager::lastError() const {
    return m_repository->lastError();
}

QString GitManager::repoPath() const {
    return m_repository->path();
}

// Ветки

QStringList GitManager::branches() const {
    return GitInternal::BranchService::branches(*m_repository);
}

QString GitManager::currentBranch() const {
    return GitInternal::BranchService::currentBranch(*m_repository);
}

bool GitManager::checkoutBranch(const QString& branchName) {
    const bool success = GitInternal::BranchService::checkout(*m_repository, branchName);
    if (success)
        emit repositoryChanged();
    return success;
}

bool GitManager::createBranch(const QString& branchName) {
    const bool success = GitInternal::BranchService::create(*m_repository, branchName);
    if (success)
        emit repositoryChanged();
    return success;
}

bool GitManager::deleteBranch(const QString& branchName) {
    const bool success = GitInternal::BranchService::remove(*m_repository, branchName);
    if (success)
        emit repositoryChanged();
    return success;
}

bool GitManager::renameBranch(const QString& oldName, const QString& newName) {
    const bool success = GitInternal::BranchService::rename(
        *m_repository, oldName, newName);
    if (success)
        emit repositoryChanged();
    return success;
}

// Коммиты

bool GitManager::createCommit(const QString& message) {
    const bool success = GitInternal::CommitService::create(*m_repository, message);
    if (success)
        emit repositoryChanged();
    return success;
}

QStringList GitManager::commitHistory(int count) const {
    return GitInternal::CommitService::history(*m_repository, count);
}

QString GitManager::commitMessage(const QString& oid) const {
    return GitInternal::CommitService::message(*m_repository, oid);
}

QString GitManager::commitAuthor(const QString& oid) const {
    return GitInternal::CommitService::author(*m_repository, oid);
}

bool GitManager::checkoutCommit(const QString& oid) {
    const bool success = GitInternal::CommitService::checkout(*m_repository, oid);
    if (success)
        emit repositoryChanged();
    return success;
}

bool GitManager::resetHard(const QString& oid) {
    const bool success = GitInternal::CommitService::resetHard(*m_repository, oid);
    if (success)
        emit repositoryChanged();
    return success;
}

bool GitManager::resetMixed(const QString& oid) {
    const bool success = GitInternal::CommitService::resetMixed(*m_repository, oid);
    if (success)
        emit repositoryChanged();
    return success;
}

bool GitManager::revertCommit(const QString& oid) {
    const bool success = GitInternal::CommitService::revert(*m_repository, oid);
    if (success)
        emit repositoryChanged();
    return success;
}

bool GitManager::amendCommit(const QString& message) {
    const bool success = GitInternal::CommitService::amend(*m_repository, message);
    if (success)
        emit repositoryChanged();
    return success;
}

// Синхронизация

bool GitManager::push(const QString& remote, const QString& branch) {
    return GitInternal::RemoteService::push(*m_repository, remote, branch);
}

bool GitManager::pull(const QString& remote, const QString& branch) {
    const bool success = GitInternal::RemoteService::pull(*m_repository, remote, branch);
    if (success)
        emit repositoryChanged();
    return success;
}

bool GitManager::fetch(const QString& remote) {
    return GitInternal::RemoteService::fetch(*m_repository, remote);
}

// Слияние

bool GitManager::merge(const QString& branchName) {
    const bool success = GitInternal::MergeService::merge(*m_repository, branchName);
    if (success)
        emit repositoryChanged();
    return success;
}

bool GitManager::hasConflicts() const {
    return GitInternal::IndexService::hasConflicts(*m_repository);
}

QStringList GitManager::conflictFiles() const {
    return GitInternal::IndexService::conflictFiles(*m_repository);
}

// Индексация

bool GitManager::stageFile(const QString& filePath) {
    const bool success = GitInternal::IndexService::stage(*m_repository, filePath);
    if (success)
        emit repositoryChanged();
    return success;
}

bool GitManager::unstageFile(const QString& filePath) {
    const bool success = GitInternal::IndexService::unstage(*m_repository, filePath);
    if (success)
        emit repositoryChanged();
    return success;
}

QString GitManager::fileDiff(const QString& filePath) const {
    return GitInternal::IndexService::fileDiff(*m_repository, filePath);
}

QString GitManager::stagedDiff() const {
    return GitInternal::IndexService::stagedDiff(*m_repository);
}

// Репозиторий

bool GitManager::clone(const QString& url, const QString& path) {
    const bool success = m_repository->clone(url, path);
    if (success)
        emit repositoryChanged();
    return success;
}

bool GitManager::init(const QString& path) {
    const bool success = m_repository->init(path);
    if (success)
        emit repositoryChanged();
    return success;
}

QString GitManager::findGitRepositoryRoot(const QString& path) {
    return GitInternal::Repository::discoverRoot(path);
}

QVector<BlameLineInfo> GitManager::blameFile(const QString& relativeFilePath) const {
    return GitInternal::BlameEngine::blameFile(*m_repository, relativeFilePath);
}

// Дополнительно

QString GitManager::status() const {
    return GitInternal::IndexService::status(*m_repository, currentBranch());
}

bool GitManager::stashSave(const QString& message) {
    const bool success = GitInternal::StashService::save(*m_repository, message);
    if (success)
        emit repositoryChanged();
    return success;
}

bool GitManager::stashApply(int index) {
    const bool success = GitInternal::StashService::apply(*m_repository, index);
    if (success)
        emit repositoryChanged();
    return success;
}

bool GitManager::stashDrop(int index) {
    const bool success = GitInternal::StashService::drop(*m_repository, index);
    if (success)
        emit repositoryChanged();
    return success;
}

QStringList GitManager::stashList() const {
    return GitInternal::StashService::list(*m_repository);
}

QString GitManager::logGraph(int count) const {
    return GitInternal::CommitService::graph(*m_repository, count);
}
