// SPDX-License-Identifier: MIT
#ifndef DISASMSYNTAXDELEGATE_H
#define DISASMSYNTAXDELEGATE_H

#include <QStyledItemDelegate>
#include <QColor>

struct DisasmSyntaxColors {
    QColor addr;
    QColor bytes;
    QColor mnemonic;
    QColor reg;
    QColor imm;
    QColor bracket;
    QColor sym;
    QColor comment;
};

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
    
    void setColors(const DisasmSyntaxColors &colors);

private:
    DisasmSyntaxColors m_colors;
    QString htmlForCell(int column, const QString &text, bool selected) const;
};

#endif // DISASMSYNTAXDELEGATE_H

