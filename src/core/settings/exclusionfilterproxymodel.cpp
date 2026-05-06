#include "exclusionfilterproxymodel.h"
#include "core/settings/appsettings.h"

#include <QFileSystemModel>
#include <QMessageBox>
#include <QPushButton>

ExclusionFilterProxyModel::ExclusionFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    reloadPatterns();
    connect(SettingsNotifier::instance(), &SettingsNotifier::excludedPatternsChanged,
            this, &ExclusionFilterProxyModel::reloadPatterns);
}

bool ExclusionFilterProxyModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
    const QModelIndex &parent) {
    if (action == Qt::MoveAction) {
        QMessageBox confirm_move(
            QMessageBox::Question,
            tr("Confirm move"),
            tr("Are you sure you want to move selected files?"),
            QMessageBox::NoButton
        );
        auto const yes = confirm_move.addButton(tr("Yes"), QMessageBox::YesRole);
        confirm_move.addButton(tr("No"), QMessageBox::NoRole);
        confirm_move.exec();
        auto const reply = confirm_move.clickedButton();
        if (reply != yes) return false;
    }

    return QSortFilterProxyModel::dropMimeData(data, action, row, column, parent);
}

void ExclusionFilterProxyModel::reloadPatterns()
{
    const QStringList raw = AppSettings::excludedPatterns();
    m_patterns.clear();
    m_patterns.reserve(raw.size());
    for (const QString &p : raw) {
        QString normalized = p.toLower().replace('\\', '/');
        while (normalized.endsWith('/'))
            normalized.chop(1);
        if (!normalized.isEmpty())
            m_patterns.append(normalized);
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    endFilterChange();
#else
    invalidateFilter();
#endif
}

bool ExclusionFilterProxyModel::filterAcceptsRow(const int sourceRow,
                                                  const QModelIndex &sourceParent) const
{
    if (m_patterns.isEmpty())
        return true;

    auto const *fsModel = qobject_cast<QFileSystemModel *>(sourceModel());
    if (!fsModel)
        return true;

    const QModelIndex idx = fsModel->index(sourceRow, 0, sourceParent);
    if (!idx.isValid())
        return true;

    const QString rawName = fsModel->fileName(idx);
    const QString name = rawName.toLower().replace('\\', '/');

    const QString fullPath = fsModel->filePath(idx).toLower().replace('\\', '/');
    const QString rootPath = fsModel->rootPath().toLower().replace('\\', '/');

    QString relativePath = fullPath;
    if (relativePath.startsWith(rootPath)) {
        relativePath = relativePath.mid(rootPath.length());
        if (relativePath.startsWith('/'))
            relativePath = relativePath.mid(1);
    }

    for (const QString &pattern : m_patterns) {
        if (pattern.contains('/')) {
            if (globMatch(pattern, relativePath))
                return false;
        } else {
            if (globMatch(pattern, name))
                return false;
        }
    }

    return true;
}

bool ExclusionFilterProxyModel::globMatch(const QString &pattern, const QString &text)
{
    // simple iterative glob matcher supporting '*' and '?'
    int px = 0;
    int tx = 0;
    int starPx = -1;
    int starTx = -1;
    const int pLen = pattern.length();
    const int tLen = text.length();

    while (tx < tLen) {
        if (px < pLen && (pattern[px] == text[tx] || pattern[px] == '?')) {
            ++px;
            ++tx;
        } else if (px < pLen && pattern[px] == '*') {
            starPx = px++;
            starTx = tx;
        } else if (starPx >= 0) {
            px = starPx + 1;
            tx = ++starTx;
        } else {
            return false;
        }
    }
    while (px < pLen && pattern[px] == '*')
        ++px;
    return px == pLen;
}
