#include "filetreeview.h"
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QMimeData>
#include <QDataStream>


FileTreeView::FileTreeView(QWidget *parent)
    : QTreeView(parent)
{
    m_expandTimer = new QTimer(this);
    m_expandTimer->setInterval(600);
    m_expandTimer->setSingleShot(true);
    connect(m_expandTimer, &QTimer::timeout, this, [this]() {
        if (m_hoverIndex.isValid()) {
            expand(m_hoverIndex);
        }
    });
}

void FileTreeView::mousePressEvent(QMouseEvent *event)
{
    QModelIndex index = this->indexAt(event->position().toPoint());
    if (index.isValid())
        emit mouseClicked(index, event->button());

    QTreeView::mousePressEvent(event);
}

void FileTreeView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() ||
        event->mimeData()->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist"))) {
        event->acceptProposedAction();
    } else {
        QTreeView::dragEnterEvent(event);
    }
}

void FileTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    QModelIndex currentHover = indexAt(event->position().toPoint());

    if (currentHover != m_hoverIndex) {
        m_hoverIndex = currentHover;
        if (m_expandTimer->isActive()) {
            m_expandTimer->stop();
        }

        QAbstractItemModel *raw = model();
        auto *proxy = qobject_cast<QSortFilterProxyModel *>(raw);
        QFileSystemModel *fsModel = proxy
            ? qobject_cast<QFileSystemModel *>(proxy->sourceModel())
            : qobject_cast<QFileSystemModel *>(raw);
        QModelIndex srcIdx = proxy ? proxy->mapToSource(m_hoverIndex) : m_hoverIndex;

        if (fsModel && fsModel->isDir(srcIdx) && !isExpanded(m_hoverIndex)) {
            m_expandTimer->start();
        }
    }

    if (event->mimeData()->hasUrls() ||
        event->mimeData()->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist"))) {
        event->acceptProposedAction();
    } else {
        QTreeView::dragMoveEvent(event);
    }
}

void FileTreeView::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();

    QAbstractItemModel *raw = model();
    auto *proxy = qobject_cast<QSortFilterProxyModel *>(raw);
    QFileSystemModel *fsModel = proxy
        ? qobject_cast<QFileSystemModel *>(proxy->sourceModel())
        : qobject_cast<QFileSystemModel *>(raw);
    if (!fsModel) {
        event->ignore();
        return;
    }

    QStringList sourcePaths;

    if (mimeData->hasUrls()) {
        for (const QUrl &url : mimeData->urls()) {
            if (url.isLocalFile())
                sourcePaths << url.toLocalFile();
        }
    } else if (mimeData->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist"))) {
        QByteArray encoded = mimeData->data(QStringLiteral("application/x-qabstractitemmodeldatalist"));
        QDataStream stream(&encoded, QIODevice::ReadOnly);
        while (!stream.atEnd()) {
            int row, col;
            QMap<int, QVariant> roleDataMap;
            stream >> row >> col >> roleDataMap;
            if (col == 0) {
                QModelIndexList selected = selectionModel()->selectedIndexes();
                for (const QModelIndex &idx : selected) {
                    if (idx.column() == 0) {
                        QModelIndex srcIdx = proxy ? proxy->mapToSource(idx) : idx;
                        QString p = fsModel->filePath(srcIdx);
                        if (!p.isEmpty() && !sourcePaths.contains(p))
                            sourcePaths << p;
                    }
                }
                break;
            }
        }
    }

    if (sourcePaths.isEmpty()) {
        event->ignore();
        return;
    }

    QModelIndex targetProxyIndex = indexAt(event->position().toPoint());
    QModelIndex targetIndex = proxy ? proxy->mapToSource(targetProxyIndex) : targetProxyIndex;
    QString targetDirPath;

    if (!targetIndex.isValid()) {
        targetDirPath = fsModel->rootPath();
    } else if (fsModel->isDir(targetIndex)) {
        targetDirPath = fsModel->filePath(targetIndex);
    } else {
        QFileInfo fi(fsModel->filePath(targetIndex));
        targetDirPath = fi.absolutePath();
    }

    if (targetDirPath.isEmpty()) {
        event->ignore();
        return;
    }

    bool movedAny = false;

    for (const QString &sourcePath : sourcePaths) {
        QFileInfo sourceInfo(sourcePath);
        QString targetPath = QDir(targetDirPath).filePath(sourceInfo.fileName());

        if (sourcePath == targetPath) continue;

        if (sourceInfo.isDir() && targetDirPath.startsWith(sourcePath)) {
            QMessageBox::warning(this, tr("Error"), tr("Cannot move a folder into itself."));
            continue;
        }

        QString msg = tr("Are you sure you want to move '%1' into '%2'?")
                      .arg(sourceInfo.fileName(), QFileInfo(targetDirPath).fileName());

        auto reply = QMessageBox::question(this, tr("Confirm Move"), msg,
                                           QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            if (QFile::rename(sourcePath, targetPath)) {
                movedAny = true;
                m_undoMoveHistory.append(qMakePair(targetPath, sourcePath));
            } else {
                QMessageBox::warning(this, tr("Error"), tr("Failed to move the file."));
            }
        }
    }

    if (movedAny) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void FileTreeView::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_Z) {
        if (!m_undoMoveHistory.isEmpty()) {
            QPair<QString, QString> lastMove = m_undoMoveHistory.takeLast();
            QString currentPath = lastMove.first;
            QString originalPath = lastMove.second;

            if (QFile::exists(currentPath)) {
                if (!QFile::rename(currentPath, originalPath)) {
                    QMessageBox::warning(this, tr("Undo Failed"), tr("Could not move the file back."));
                    m_undoMoveHistory.append(lastMove);
                }
            } else {
                QMessageBox::warning(this, tr("Undo Failed"), tr("The file no longer exists at the moved location."));
            }
        }
        event->accept();
        return;
    }

    QTreeView::keyPressEvent(event);
}
