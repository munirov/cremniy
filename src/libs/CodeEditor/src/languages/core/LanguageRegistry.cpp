#include "languages/LanguageRegistry.h"

#include <QtGlobal>

#include <algorithm>

namespace {

QString normalizeKey(const QString& value)
{
    return value.trimmed().toLower();
}

} // namespace

LanguageRegistry& LanguageRegistry::instance()
{
    static LanguageRegistry registry;
    return registry;
}

void LanguageRegistry::registerLanguage(LanguageDefinition definition)
{
    Q_ASSERT_X(!definition.id.trimmed().isEmpty(), "LanguageRegistry::registerLanguage",
               "language id must not be empty");

    const QString id = normalizeKey(definition.id);
    definition.id = id;

    for (const QString& ext : std::as_const(definition.fileExtensions))
        m_idByExtension.insert(normalizeKey(ext), id);

    for (const QString& fileName : std::as_const(definition.exactFileNames))
        m_idByExactFileName.insert(normalizeKey(fileName), id);

    m_byId.insert(id, std::move(definition));
}

const LanguageDefinition* LanguageRegistry::find(const QString& id) const
{
    auto it = m_byId.find(normalizeKey(id));
    if (it == m_byId.end())
        return nullptr;
    return &it.value();
}

const LanguageDefinition& LanguageRegistry::resolveForFile(const QString& fileName) const
{
    const QString baseName = fileName.section(QLatin1Char('/'), -1);

    auto exactIt = m_idByExactFileName.find(normalizeKey(baseName));
    if (exactIt != m_idByExactFileName.end()) {
        if (const LanguageDefinition* def = find(exactIt.value()))
            return *def;
    }

    const int dotIndex = baseName.lastIndexOf(QLatin1Char('.'));
    const QString extension = dotIndex >= 0 ? baseName.mid(dotIndex + 1) : QString();
    return resolveForExtension(extension);
}

const LanguageDefinition& LanguageRegistry::resolveForExtension(const QString& extension) const
{
    auto extIt = m_idByExtension.find(normalizeKey(extension));
    if (extIt != m_idByExtension.end()) {
        if (const LanguageDefinition* def = find(extIt.value()))
            return *def;
    }
    return plainTextLanguage();
}

QVector<const LanguageDefinition*> LanguageRegistry::allLanguages() const
{
    QVector<const LanguageDefinition*> result;
    result.reserve(m_byId.size());
    for (const auto& def : m_byId)
        result.append(&def);

    std::sort(result.begin(), result.end(), [](const LanguageDefinition* a, const LanguageDefinition* b) {
        return a->displayName.localeAwareCompare(b->displayName) < 0;
    });

    return result;
}

const LanguageDefinition& LanguageRegistry::plainTextLanguage()
{
    static const LanguageDefinition plain{
        QStringLiteral("text"),
        QStringLiteral("Plain Text"),
        {QStringLiteral("txt")},
        {},
        QString(),
        [](QTextDocument*) -> QStyleSyntaxHighlighter* { return nullptr; }
    };
    return plain;
}
