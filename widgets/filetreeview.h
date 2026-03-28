#ifndef FILETREEVIEW_H
#define FILETREEVIEW_H

#include <QTreeView>
#include <QMouseEvent>


class FileTreeView : public QTreeView {
    Q_OBJECT

public:
    explicit FileTreeView(QWidget *parent = nullptr);

signals:
    void mouseClicked(QModelIndex index, Qt::MouseButton button);

protected:
    void mousePressEvent(QMouseEvent *event) override;
};


#endif // FILETREEVIEW_H