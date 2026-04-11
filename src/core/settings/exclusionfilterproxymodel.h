#ifndef EXCLUSIONFILTERPROXYMODEL_H
#define EXCLUSIONFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QStringList>

class ExclusionFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ExclusionFilterProxyModel(QObject *parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;

private slots:
    void reloadPatterns();

private:
    static bool globMatch(const QString &pattern, const QString &text);

    QStringList m_patterns;
};

#endif // EXCLUSIONFILTERPROXYMODEL_H
