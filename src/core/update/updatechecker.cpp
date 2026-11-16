#include "updatechecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "config.h"

namespace core {

namespace {

const QString GITHUB_RELEASES_URL = QStringLiteral(
    "https://api.github.com/repos/munirov/cremniy/releases/latest"
);

const QString GITHUB_ACCEPT_HEADER = QStringLiteral("application/vnd.github+json");
const QString GITHUB_USER_AGENT = QStringLiteral("Cremniy-IDE");

} /* namespace */

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

void UpdateChecker::checkForUpdate()
{
    QNetworkRequest request{QUrl(GITHUB_RELEASES_URL)};

    request.setRawHeader("Accept", GITHUB_ACCEPT_HEADER.toUtf8());
    request.setRawHeader("User-Agent", GITHUB_USER_AGENT.toUtf8());

    auto* reply = m_networkManager->get(request);

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply]() {
            handleReply(reply);
        }
    );
}

QString UpdateChecker::normalizeVersion(const QString& version)
{
    QString normalized = version.trimmed();

    if (normalized.startsWith('v')) {
        normalized.remove(0, 1);
    }

    return normalized;
}

bool UpdateChecker::isNewerVersion(const QString& current, const QString& latest)
{
    const QStringList currentParts = current.split('.');
    const QStringList latestParts = latest.split('.');

    const int maxParts = qMax(currentParts.size(), latestParts.size());

    for (int i = 0; i < maxParts; ++i) {
        const int currentPart = i < currentParts.size() ? currentParts.at(i).toInt() : 0;
        const int latestPart = i < latestParts.size() ? latestParts.at(i).toInt() : 0;

        if (latestPart > currentPart) {
            return true;
        }

        if (latestPart < currentPart) {
            return false;
        }
    }

    return false;
}

void UpdateChecker::handleReply(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit checkFailed(reply->errorString());
        return;
    }

    const QByteArray responseData = reply->readAll();

    const QJsonDocument document = QJsonDocument::fromJson(responseData);

    if (!document.isObject()) {
        emit checkFailed(QStringLiteral("Invalid JSON response from GitHub API."));
        return;
    }

    const QJsonObject rootObject = document.object();

    if (!rootObject.contains("name")) {
        emit checkFailed(QStringLiteral("Release name is missing in the response."));
        return;
    }

    const QString latestVersion = normalizeVersion(rootObject["name"].toString());

    if (latestVersion.isEmpty()) {
        emit checkFailed(QStringLiteral("Release name is empty in the response."));
        return;
    }

    const QString currentVersion = normalizeVersion(QStringLiteral(CREMNIY_VERSION));

    if (isNewerVersion(currentVersion, latestVersion)) {
        emit updateAvailable(latestVersion);
    }
}

} /* namespace core */