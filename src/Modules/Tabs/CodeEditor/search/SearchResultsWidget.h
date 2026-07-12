#ifndef SEARCHRESULTSWIDGET_H
#define SEARCHRESULTSWIDGET_H

#include <QTreeWidget>
#include "SearchDefs.h"

class SearchResultsWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit SearchResultsWidget(QWidget* parent = nullptr);

    void addSingleResult(const SearchResult& result);
    void clearResults();
    int totalMatches() const { return m_totalMatches; }
    int totalFileCount() const { return m_fileGroups.size(); }

signals:
    void resultActivated(const SearchResult& result);

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    QTreeWidgetItem* findOrCreateFileGroup(const QString& filePath);

    QMap<QString, QTreeWidgetItem*> m_fileGroups;
    int m_totalMatches = 0;
};

#endif // SEARCHRESULTSWIDGET_H
