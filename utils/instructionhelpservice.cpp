#include "instructionhelpservice.h"

#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

InstructionHelpService &InstructionHelpService::instance()
{
    static InstructionHelpService s;
    return s;
}

InstructionHelpService::InstructionHelpService()
{
    loadLocale("ru");
}

void InstructionHelpService::loadLocale(const QString &locale)
{
    m_entries.clear();

    const QString externalPath = QDir(QCoreApplication::applicationDirPath())
                                     .filePath(QString("instructions_%1.json").arg(locale));
    const QString resourcePath = QString(":/data/instructions_%1.json").arg(locale);
    QFile f(QFile::exists(externalPath) ? externalPath : resourcePath);
    if (!f.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;

    const QJsonArray arr = doc.object().value("instructions").toArray();
    for (const QJsonValue &v : arr) {
        if (!v.isObject())
            continue;
        const QJsonObject o = v.toObject();
        InstructionHelpEntry e;
        e.mnemonic = normalizeMnemonic(o.value("mnemonic").toString());
        if (e.mnemonic.isEmpty())
            continue;
        e.title = o.value("title").toString();
        e.description = o.value("description").toString();
        const QJsonArray flags = o.value("flags").toArray();
        for (const QJsonValue &fv : flags)
            e.flags << fv.toString();
        m_entries.insert(e.mnemonic, e);
    }
}

QString InstructionHelpService::normalizeMnemonic(const QString &value)
{
    QString out = value.trimmed().toLower();
    out.remove(QRegularExpression("[^a-z0-9]"));
    return out;
}

QString InstructionHelpService::extractMnemonicFromLine(const QString &line)
{
    QString tail = line;
    const int colon = tail.indexOf(':');
    if (colon >= 0)
        tail = tail.mid(colon + 1);
    tail = tail.trimmed();

    const QRegularExpression re("([A-Za-z]{2,12})");
    const QRegularExpressionMatch m = re.match(tail);
    if (!m.hasMatch())
        return {};
    return normalizeMnemonic(m.captured(1));
}

QStringList InstructionHelpService::extractNumericTokens(const QString &text)
{
    QStringList out;
    static const QRegularExpression re(
        R"((0x[0-9a-fA-F]+)|([0-9a-fA-F]+h\b)|(0b[01]+)|([0-7]+o\b)|(\b\d+\b))");
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const QString t = it.next().captured(0).trimmed();
        if (!t.isEmpty())
            out << t;
    }
    out.removeDuplicates();
    return out;
}

bool InstructionHelpService::parseAsmNumber(const QString &token, qulonglong *out)
{
    if (!out)
        return false;
    QString s = token.trimmed().toLower();
    bool ok = false;
    qulonglong v = 0;

    if (s.startsWith("0x")) {
        v = s.mid(2).toULongLong(&ok, 16);
    } else if (s.endsWith('h')) {
        v = s.left(s.size() - 1).toULongLong(&ok, 16);
    } else if (s.startsWith("0b")) {
        v = s.mid(2).toULongLong(&ok, 2);
    } else if (s.endsWith('o')) {
        v = s.left(s.size() - 1).toULongLong(&ok, 8);
    } else {
        v = s.toULongLong(&ok, 10);
    }

    if (!ok)
        return false;
    *out = v;
    return true;
}

QString InstructionHelpService::makeNumberRow(const QString &token)
{
    qulonglong v = 0;
    if (!parseAsmNumber(token, &v))
        return {};
    return QString("<li><code>%1</code> -> dec: <b>%2</b>, oct: <b>%3</b>, hex: <b>0x%4</b></li>")
        .arg(token)
        .arg(QString::number(v, 10))
        .arg(QString::number(v, 8))
        .arg(QString::number(v, 16).toUpper());
}

QString InstructionHelpService::tooltipForToken(const QString &token, const QString &lineContext) const
{
    const QString key = normalizeMnemonic(token);
    if (key.isEmpty() || !m_entries.contains(key))
        return {};

    const InstructionHelpEntry &e = m_entries[key];
    QString html;
    html += QString("<b>%1</b>").arg(e.title.isEmpty() ? key.toUpper() : e.title);
    if (!e.description.isEmpty())
        html += QString("<br/>%1").arg(e.description.toHtmlEscaped());

    html += "<br/><br/><b>Флаги:</b> ";
    html += e.flags.isEmpty() ? "не изменяет" : e.flags.join(", ");

    const QStringList nums = extractNumericTokens(lineContext);
    if (!nums.isEmpty()) {
        html += "<br/><br/><b>Числа в разных системах:</b><ul style='margin:4px 0 0 14px; padding:0;'>";
        for (const QString &n : nums) {
            const QString row = makeNumberRow(n);
            if (!row.isEmpty())
                html += row;
        }
        html += "</ul>";
    }
    return html;
}

QString InstructionHelpService::tooltipForLine(const QString &lineContext) const
{
    const QString mnem = extractMnemonicFromLine(lineContext);
    if (mnem.isEmpty())
        return {};
    return tooltipForToken(mnem, lineContext);
}
