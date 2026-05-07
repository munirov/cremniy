#ifndef FILETREEPANEL_H
#define FILETREEPANEL_H

#include <QTreeView>
#include <QFileSystemModel>
#include <QPointer>
#include <QVBoxLayout>


class QSortFilterProxyModel;

class FileTreePanel : public QWidget {
    Q_OBJECT

public:
    explicit FileTreePanel(QWidget* parent, QFileSystemModel* model, QSortFilterProxyModel* proxy, const QString& rootPath);

signals:
    void openFileRequested(const QString& filePath, const QString& fileName);

private slots:
    void showMenu(const QPoint& point) const;

private:
    void setupUi() const;
    void setupModel() const;
    void setupContextMenu();
    void setupConnections();

    void open();
    void remove() const;
    [[nodiscard]] QString currentPath() const;
    [[nodiscard]] QModelIndex getSourceIndex() const;

    QVBoxLayout* m_layout;
    QTreeView* m_treeView;
    QSortFilterProxyModel* m_proxy;
    QFileSystemModel* m_fileModel;

    QAction* m_createFile{};
    QAction* m_createDir{};
    QAction* m_open{};
    QAction* m_rename{};
    QAction* m_delete{};

    const QString m_root_path;
};
#endif // FILETREEPANEL_H
