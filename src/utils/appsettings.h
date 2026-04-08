#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QJsonObject>
#include <QString>

class AppSettings
{
public:
    enum class DisasmBackend {
        Objdump = 0,
        Radare2 = 1,
    };

    //
    static QJsonObject getSettingsJson();
    static void updateSettingsJson(const QJsonObject& data);

    static DisasmBackend disasmBackend();
    static void setDisasmBackend(DisasmBackend backend);

    static QString objdumpPath();
    static void setObjdumpPath(const QString &path);

    static QString radare2Path();
    static void setRadare2Path(const QString &path);

    enum class Radare2AnalysisLevel {
        None = 0,
        Aa   = 1,
        Aaa  = 2,
    };

    enum class AsmSyntax {
        Intel = 0,
        Att   = 1,
    };

    static int disasmInsnLimitPerSection();
    static void setDisasmInsnLimitPerSection(int limit);

    static Radare2AnalysisLevel radare2AnalysisLevel();
    static void setRadare2AnalysisLevel(Radare2AnalysisLevel lvl);

    static AsmSyntax asmSyntax();
    static void setAsmSyntax(AsmSyntax syntax);

    // Optional commands executed in r2 before JSON queries (semicolon-separated).
    static QString radare2PreCommands();
    static void setRadare2PreCommands(const QString &cmds);

    // Import/export settings to share with others (INI file).
    static bool exportToIni(const QString &filePath, QString *error = nullptr);
    static bool importFromIni(const QString &filePath, QString *error = nullptr);

private:
    static QString getAppSettingsPath();
    static QString keyDisasmBackend();
    static QString keyObjdumpPath();
    static QString keyRadare2Path();
    static QString keyInsnLimitPerSection();
    static QString keyRadare2AnalysisLevel();
    static QString keyAsmSyntax();
    static QString keyRadare2PreCommands();
};

#endif // APPSETTINGS_H

