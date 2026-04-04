#ifndef PROJECTSEARCHRESULTDELEGATE_H
#define PROJECTSEARCHRESULTDELEGATE_H

#include <QStyledItemDelegate>

class ProjectSearchResultDelegate : public QStyledItemDelegate
{
public:
    explicit ProjectSearchResultDelegate(QObject *parent = nullptr);

    void setNeedle(const QString &needle);
    void setPreviewColumn(int column);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    QString m_needle;
    int m_previewColumn = 1;
};

#endif
