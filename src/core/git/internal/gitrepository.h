#pragma once

#include <QString>

struct git_repository;
struct git_signature;

namespace GitInternal {

    /**
 * Owns the libgit2 repository handle and the error state shared by Git
 * operation services. No UI or application workflow belongs here.
 */
    class Repository final {
    public:
        Repository();
        ~Repository();

        Repository(const Repository&) = delete;
        Repository& operator=(const Repository&) = delete;

        bool open(const QString& path);
        bool clone(const QString& url, const QString& path);
        bool init(const QString& path);
        void close();
        void adopt(git_repository* repository, const QString& path);

        bool isOpen() const;
        git_repository* handle() const;
        QString path() const;

        QString lastError() const;
        void setError(const QString& error) const;
        void clearError() const;

        git_signature* createSignature() const;

        static QString discoverRoot(const QString& path);

    private:
        QString userName() const;
        QString userEmail() const;

        git_repository* m_repository = nullptr;
        QString m_path;
        mutable QString m_lastError;
    };

}// namespace GitInternal
