#ifndef KEYBOARDSCANCODEVIZWIDGET_H
#define KEYBOARDSCANCODEVIZWIDGET_H

#include <QVector>
#include <QWidget>

class QFrame;
class QKeyEvent;

class KeyboardScanCodeVizWidget : public QWidget
{
    Q_OBJECT
public:
    explicit KeyboardScanCodeVizWidget(QWidget *parent = nullptr);

    void applyHighlight(const QKeyEvent *e);
    void clearHighlight();

    struct Cell {
        int qtKey;
        int qtKeyAlt;
        int8_t keypadFilter;
        QFrame *frame;
    };

private:

    QVector<Cell> m_cells;
    QVector<QFrame *> m_highlighted;
};

#endif
