#include "SearchResultsWidget.h"
#include <QFileInfo>
#include <QFont>

SearchResultsWidget::SearchResultsWidget(QWidget* parent)
    : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setRootIsDecorated(true);
    setIndentation(16);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setExpandsOnDoubleClick(true);

    connect(this, &QTreeWidget::itemDoubleClicked, this, &SearchResultsWidget::onItemDoubleClicked);
}

void SearchResultsWidget::addSingleResult(const SearchResult& result)
{
    QTreeWidgetItem* group = findOrCreateFileGroup(result.filePath);

    auto* item = new QTreeWidgetItem(group);
    item->setText(0, QString("  %1: %2").arg(result.lineNumber).arg(result.lineText.trimmed()));
    item->setData(0, Qt::UserRole, QVariant::fromValue(result));
    item->setToolTip(0, QString("%1:%2").arg(result.filePath).arg(result.lineNumber));

    // Style the line number portion differently
    QFont monoFont("JetBrains Mono", 9);
    item->setFont(0, monoFont);

    m_totalMatches++;

    // Update group text with new count
    QFileInfo fi(result.filePath);
    group->setText(0, QString("%1 (%2 matches)").arg(fi.fileName()).arg(group->childCount()));
}

void SearchResultsWidget::clearResults()
{
    clear();
    m_fileGroups.clear();
    m_totalMatches = 0;
}

QTreeWidgetItem* SearchResultsWidget::findOrCreateFileGroup(const QString& filePath)
{
    auto it = m_fileGroups.find(filePath);
    if (it != m_fileGroups.end())
        return it.value();

    QFileInfo fi(filePath);
    auto* group = new QTreeWidgetItem(this);
    group->setText(0, QString("%1 (0 matches)").arg(fi.fileName()));
    group->setExpanded(true);

    QFont boldFont = group->font(0);
    boldFont.setBold(true);
    group->setFont(0, boldFont);

    group->setToolTip(0, filePath);

    m_fileGroups.insert(filePath, group);
    return group;
}

void SearchResultsWidget::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (!item)
        return;

    QVariant data = item->data(0, Qt::UserRole);
    if (data.canConvert<SearchResult>()) {
        SearchResult result = data.value<SearchResult>();
        emit resultActivated(result);
    }
}
