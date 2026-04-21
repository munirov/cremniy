
#include <QHeaderView>
#include <utility>

#include "filetreepanel.h"

#include <QMenu>

#include "exclusionfilterproxymodel.h"
#include "core/icons/iconprovider.h"
#include "dialogs/filecreatedialog.h"


FileTreePanel::FileTreePanel(QWidget* parent, const QString& rootPath)
        : QWidget(parent),
          m_layout(new QVBoxLayout(this)),
          m_treeView(new QTreeView(this)),
          m_exclusionProxy(new ExclusionFilterProxyModel()),
          m_fileModel(new QFileSystemModel(this)),
          m_root_path(rootPath){
    setupModel();
    setupUi();
    setupContextMenu();
    setupConnections();
}

void FileTreePanel::setupModel() const {
    m_fileModel->setRootPath(m_root_path);
    m_fileModel->setIconProvider(new IconProvider());
    m_exclusionProxy->setSourceModel(m_fileModel);
    m_treeView->setModel(m_exclusionProxy);
    m_treeView->setRootIndex(
        m_exclusionProxy->mapFromSource(
            m_fileModel->index(m_root_path)
        )
    );
}

void FileTreePanel::setupUi() const {
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_treeView, 1);

    m_treeView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_fileModel->setReadOnly(false);

    m_treeView->setMinimumWidth(180);
    m_treeView->setTextElideMode(Qt::ElideNone);
    m_treeView->setIndentation(12);

    m_treeView->setColumnHidden(1, true);
    m_treeView->setColumnHidden(2, true);
    m_treeView->setColumnHidden(3, true);

    m_treeView->header()->hide();
    m_treeView->setAnimated(true);
    m_treeView->setEditTriggers(QAbstractItemView::EditKeyPressed);

    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    m_treeView->setDragDropOverwriteMode(false);
    m_treeView->setDragEnabled(true);
    m_treeView->setAcceptDrops(true);
    m_treeView->setDropIndicatorShown(true);
    m_treeView->setDragDropMode(QAbstractItemView::DragDrop);
    m_treeView->setDefaultDropAction(Qt::MoveAction);

    m_treeView->setMouseTracking(true);
}

void FileTreePanel::setupContextMenu() {
    m_createFile = new QAction(tr("Create File"), this);
    m_createDir  = new QAction(tr("Create Folder"), this);
    m_delete     = new QAction(tr("Delete"), this);
    m_rename     = new QAction(tr("Rename"), this);
    m_open       = new QAction(tr("Open"), this);
}

void FileTreePanel::setupConnections() {
    connect(m_treeView, &QWidget::customContextMenuRequested, this, &FileTreePanel::showMenu);
    connect(m_treeView, &QTreeView::doubleClicked, this, &FileTreePanel::open);

    connect(m_open, &QAction::triggered, this, &FileTreePanel::open);
    connect(m_createDir, &QAction::triggered, this, [this] {
        FileCreateDialog fcd(this, currentPath(),true);
        fcd.exec();
    });
    connect(m_createFile, &QAction::triggered, this, [this] {
        FileCreateDialog fcd(this, currentPath(),false);
        fcd.exec();
    });
    connect(m_rename, &QAction::triggered, this, [this] {
        m_treeView->edit(m_treeView->currentIndex());
    });
    connect(m_delete, &QAction::triggered, this, &FileTreePanel::remove);
}

QString FileTreePanel::currentPath() const {
    return m_fileModel->filePath(getSourceIndex());
}

void FileTreePanel::open() {
    const QModelIndex srcIdx = getSourceIndex();
    const bool isDir = m_fileModel->isDir(srcIdx);
    if (isDir) m_treeView->expand(m_treeView->currentIndex());
    else emit openFileRequested(m_fileModel->filePath(srcIdx), m_fileModel->fileName(srcIdx));
}

void FileTreePanel::remove() const {
    const QString body = tr("Are you sure you want to delete the file \"%1\"?").arg(m_fileModel->fileName(getSourceIndex()));

    QMessageBox confirmRemove(QMessageBox::Question, tr("Delete"), body, QMessageBox::NoButton);
    [[maybe_unused]] const auto yes = confirmRemove.addButton(tr("Yes"), QMessageBox::YesRole);
    const auto no = confirmRemove.addButton(tr("No"), QMessageBox::NoRole);
    confirmRemove.exec();

    const auto reply = confirmRemove.clickedButton();
    if (reply == no) return;

    m_fileModel->remove(getSourceIndex());
}

QModelIndex FileTreePanel::getSourceIndex() const{
    const QModelIndex idx = m_treeView->currentIndex();
    if (!idx.isValid()) return {};
    const QModelIndex srcIdx = m_exclusionProxy->mapToSource(idx);
    return srcIdx;
}

void FileTreePanel::showMenu(const QPoint& point) const {
    const auto index = m_treeView->indexAt(point);
    const bool onItem = index.isValid();
    QMenu menu;

    if (!onItem || m_fileModel->isDir(getSourceIndex())) {
        menu.addAction(m_createDir);
        menu.addAction(m_createFile);
    }

    if (onItem) {
        menu.addAction(m_open);
        menu.addAction(m_rename);
        menu.addAction(m_delete);
    }

    if (! menu.isEmpty() ) menu.exec(m_treeView->viewport()->mapToGlobal(point));
}