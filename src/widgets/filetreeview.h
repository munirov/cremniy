#ifndef FILETREEVIEW_H
#define FILETREEVIEW_H

#include <QTreeView>
#include <QMouseEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QTimer>
#include <QKeyEvent>
#include <QList>
#include <QPair>


class FileTreeView : public QTreeView {
    Q_OBJECT

public:
    explicit FileTreeView(QWidget *parent = nullptr);

signals:
    void mouseClicked(QModelIndex index, Qt::MouseButton button);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

private:
    QTimer* m_expandTimer;
    QModelIndex m_hoverIndex;
    QList<QPair<QString, QString>> m_undoMoveHistory;
};


#endif // FILETREEVIEW_H
