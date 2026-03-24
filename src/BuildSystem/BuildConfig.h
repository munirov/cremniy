#pragma once
#include <QString>
#include <QDir>

struct BuildConfig {
    QString build;
    QString run;
    QString clean;
    QString qtPath;

    bool isEmpty() const {
        return build.isEmpty() && run.isEmpty() && clean.isEmpty() && qtPath.isEmpty();
    }
};

class BuildConfigManager {
public:
    static constexpr const char* CONFIG_FILE = "cremniy.json";

    // Загрузить конфиг из папки проекта. Возвращает false, если файл не найден.
    static bool load(const QString& projectDir, BuildConfig& out);

    // Сохранить конфиг в папку проекта.
    static bool save(const QString& projectDir, const BuildConfig& cfg);

    // Попытаться автогенерировать конфиг, сканируя папку.
    // Возвращает true и заполняет out, если найдены CMakeLists.txt или Makefile.
    static bool autoDetect(const QString& projectDir, BuildConfig& out);
    static BuildConfig defaultCMakeTemplate(const QString& projectDir);
    static QString detectQtPrefixPath();
    static void applyQtPathToCMakeConfig(BuildConfig& cfg);

private:
    static QString detectCMakeProjectName(const QString& cmakeListsPath);
    static QString detectQtCmakePackageDir(const QString& qtPrefixPath);
};
