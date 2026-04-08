#include "BuildConfig.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDirIterator>
#include <QRegularExpression>

bool BuildConfigManager::load(const QString& projectDir, BuildConfig& out) {
    QFile file(QDir(projectDir).filePath(CONFIG_FILE));
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject())
        return false;

    QJsonObject obj = doc.object();
    out.build = obj.value("build").toString();
    out.run   = obj.value("run").toString();
    out.clean = obj.value("clean").toString();
    out.qtPath = obj.value("qtPath").toString();
    return true;
}

bool BuildConfigManager::save(const QString& projectDir, const BuildConfig& cfg) {
    QJsonObject obj;
    obj["build"] = cfg.build;
    obj["run"]   = cfg.run;
    obj["clean"] = cfg.clean;
    obj["qtPath"] = cfg.qtPath;

    QFile file(QDir(projectDir).filePath(CONFIG_FILE));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

bool BuildConfigManager::autoDetect(const QString& projectDir, BuildConfig& out) {
    QDir dir(projectDir);

    if (dir.exists("CMakeLists.txt")) {
        out = defaultCMakeTemplate(projectDir);
        return true;
    }

    if (dir.exists("Makefile")) {
        out.build = "make";
        out.run = "./<target>";
        out.clean = "make clean";
        out.qtPath.clear();
        return true;
    }

    return false;
}

BuildConfig BuildConfigManager::defaultCMakeTemplate(const QString& projectDir)
{
    QDir dir(projectDir);
    QString projectName;

    if (dir.exists("CMakeLists.txt"))
        projectName = detectCMakeProjectName(dir.filePath("CMakeLists.txt"));

    if (projectName.isEmpty())
        projectName = QFileInfo(projectDir).fileName().trimmed();

    if (projectName.isEmpty())
        projectName = "app";

    BuildConfig cfg;
    cfg.qtPath = detectQtPrefixPath();

#ifdef Q_OS_WIN
    cfg.build = "cmake -S . -B build && cmake --build build --config Release";
    cfg.clean = "cmake --build build --config Release --target clean";
#else
    cfg.build = "cmake -S . -B build && cmake --build build";
    cfg.clean = "cmake --build build --target clean";
#endif

#ifdef Q_OS_WIN
    cfg.run = QString("\".\\build\\Release\\%1.exe\"").arg(projectName);
#else
    cfg.run = QString("./build/%1").arg(projectName);
#endif

    return cfg;
}

QString BuildConfigManager::detectQtPrefixPath()
{
    QString prefixPath = qEnvironmentVariable("CMAKE_PREFIX_PATH");
    if (!prefixPath.isEmpty())
        return prefixPath;

#ifdef Q_OS_WIN
    const QStringList roots = {
        "C:/Qt", "D:/Qt", "E:/Qt",
        "C:/DevTools/Qt", "D:/DevTools/Qt", "E:/DevTools/Qt"
    };

    QRegularExpression versionRe(R"(^\d+\.\d+(\.\d+)?$)");
    for (const QString& rootPath : roots) {
        QDir root(rootPath);
        if (!root.exists())
            continue;

        const QFileInfoList versions = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
        for (const QFileInfo& versionInfo : versions) {
            if (!versionRe.match(versionInfo.fileName()).hasMatch())
                continue;

            QDir versionDir(versionInfo.absoluteFilePath());
            const QFileInfoList kits = versionDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
            for (const QFileInfo& kitInfo : kits) {
                const QString kitName = kitInfo.fileName().toLower();
                if (!kitName.contains("msvc") && !kitName.contains("mingw"))
                    continue;

                const QString candidate = kitInfo.absoluteFilePath();
                if (QFileInfo::exists(QDir(candidate).filePath("lib/cmake/Qt6/Qt6Config.cmake")) ||
                    QFileInfo::exists(QDir(candidate).filePath("lib/cmake/Qt5/Qt5Config.cmake"))) {
                    return candidate;
                }
            }
        }
    }
#endif

    return {};
}

void BuildConfigManager::applyQtPathToCMakeConfig(BuildConfig&)
{
}

QString BuildConfigManager::detectQtCmakePackageDir(const QString& qtPrefixPath)
{
    const QString qt6Dir = QDir(qtPrefixPath).filePath("lib/cmake/Qt6");
    if (QFileInfo::exists(QDir(qt6Dir).filePath("Qt6Config.cmake")))
        return qt6Dir;

    const QString qt5Dir = QDir(qtPrefixPath).filePath("lib/cmake/Qt5");
    if (QFileInfo::exists(QDir(qt5Dir).filePath("Qt5Config.cmake")))
        return qt5Dir;

    return {};
}

QString BuildConfigManager::detectCMakeProjectName(const QString& cmakeListsPath)
{
    QFile file(cmakeListsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    const QString content = QString::fromUtf8(file.readAll());
    const QRegularExpression re(R"(project\s*\(\s*([^\s\)]+))", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(content);
    if (!match.hasMatch())
        return {};

    return match.captured(1).trimmed();
}
