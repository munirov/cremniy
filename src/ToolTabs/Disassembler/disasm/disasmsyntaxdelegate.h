/* SPDX-License-Identifier: MIT */
#ifndef DISASMSYNTAXDELEGATE_H
#define DISASMSYNTAXDELEGATE_H

#include <QStyledItemDelegate>

class DisasmSyntaxDelegate final : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit DisasmSyntaxDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

private:
    static QString htmlForCell(int column, const QString &text, bool selected);
};

#endif // DISASMSYNTAXDELEGATE_H

