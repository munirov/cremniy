#pragma once

#include <QFileSystemModel>
#include <QPointer>
#include <QVBoxLayout>

class QTreeView;
class QSortFilterProxyModel;
class QAction;
class QPoint;
class IconProvider;

class FileTreePanel : public QWidget {
    Q_OBJECT

public:
    explicit FileTreePanel(QWidget* parent, QFileSystemModel* model, QSortFilterProxyModel* proxy, const QString& rootPath);
    ~FileTreePanel() override;

signals:
    void openFileRequested(const QString& filePath, const QString& fileName);

private slots:
    void showMenu(const QPoint& point) const;

private:
    void setupUi();
    void setupModel();
    void setupContextMenu();
    void setupConnections();
    void open();
    void remove();
    [[nodiscard]] QString currentPath() const;
    [[nodiscard]] QModelIndex getSourceIndex() const;

    QVBoxLayout* m_layout;
    QTreeView* m_treeView;
    QSortFilterProxyModel* m_proxy;
    QFileSystemModel* m_fileModel;
    IconProvider* m_iconProvider;

    QAction* m_createFile{};
    QAction* m_createDir{};
    QAction* m_open{};
    QAction* m_rename{};
    QAction* m_delete{};

    mutable QString m_contextPath;
    const QString m_root_path;
};
