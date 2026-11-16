#ifndef CREMNIY_UPDATE_CHECKER_H
#define CREMNIY_UPDATE_CHECKER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace core {

/**
 * @brief Checks for a new Cremniy release on GitHub.
 *
 * The check is performed asynchronously via QNetworkAccessManager and does
 * not block the application startup. When a newer version is found, the
 * updateAvailable() signal is emitted. On any network or parsing error the
 * checkFailed() signal is emitted for logging purposes.
 */
class UpdateChecker : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs an UpdateChecker.
     * @param parent Parent QObject.
     */
    explicit UpdateChecker(QObject* parent = nullptr);

    /**
     * @brief Starts an asynchronous check for a new release.
     *
     * Sends a request to the GitHub REST API and compares the latest release
     * version with the current application version.
     */
    void checkForUpdate();

    /**
     * @brief Normalizes a version string by stripping a leading 'v'.
     * @param version Raw version string, e.g. "v1.2.3".
     * @return Version string without a leading 'v'.
     */
    static QString normalizeVersion(const QString& version);

    /**
     * @brief Compares two version strings.
     * @param current Current application version.
     * @param latest Latest available version.
     * @return true if latest is newer than current, false otherwise.
     */
    static bool isNewerVersion(const QString& current, const QString& latest);

signals:
    /**
     * @brief Emitted when a newer version is available.
     * @param latestVersion Latest available version.
     */
    void updateAvailable(const QString& latestVersion);

    /**
     * @brief Emitted when the check fails (network or parsing error).
     * @param reason Human-readable failure reason.
     */
    void checkFailed(const QString& reason);

private:
    QNetworkAccessManager* m_networkManager;

    /**
     * @brief Handles the finished network reply.
     * @param reply Finished network reply.
     */
    void handleReply(QNetworkReply* reply);
};

} /* namespace core */

#endif /* CREMNIY_UPDATE_CHECKER_H */