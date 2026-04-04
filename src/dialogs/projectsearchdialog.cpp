#include "projectsearchdialog.h"
#include "projectsearchresultdelegate.h"
#include "utils/projectsearch/projectsearchworker.h"

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QColor>
#include <QResizeEvent>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPushButton>
#include <QThread>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kColLine = 0;
constexpr int kColPreview = 1;
constexpr int kPathRole = Qt::UserRole;
constexpr int kLineRole = Qt::UserRole + 1;

QString fileKey(const QString &absPath)
{
    return QDir::cleanPath(QFileInfo(absPath).absoluteFilePath());
}

} // namespace

ProjectSearchDialog::~ProjectSearchDialog()
{
    stopActiveSearch();
}

void ProjectSearchDialog::setSearchQuery(const QString &text)
{
    if (!m_queryEdit)
        return;
    m_queryEdit->setText(text);
    if (!text.isEmpty()) {
        m_queryEdit->setFocus(Qt::OtherFocusReason);
        m_queryEdit->selectAll();
    }
}

ProjectSearchDialog::ProjectSearchDialog(const QString &projectRoot, QWidget *parent)
    : QDialog(parent), m_projectRoot(QDir::cleanPath(QFileInfo(projectRoot).absoluteFilePath()))
{
    setObjectName(QStringLiteral("ProjectSearchDialog"));
    setWindowTitle(tr("Find in Project"));
    setMinimumSize(680, 440);

    auto *searchLabel = new QLabel(tr("Search"), this);
    searchLabel->setObjectName(QStringLiteral("projectSearchFieldLabel"));

    m_queryEdit = new QLineEdit(this);
    m_queryEdit->setObjectName(QStringLiteral("projectSearchQuery"));
    m_queryEdit->setPlaceholderText(tr("Type a pattern…"));
    m_queryEdit->setClearButtonEnabled(true);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({QStringLiteral("Line"), QStringLiteral("Match")});
    if (QTreeWidgetItem *hdr = m_tree->headerItem()) {
        hdr->setTextAlignment(kColLine, Qt::AlignLeft | Qt::AlignVCenter);
        hdr->setTextAlignment(kColPreview, Qt::AlignLeft | Qt::AlignVCenter);
    }
    m_tree->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setMinimumSectionSize(40);
    m_tree->header()->setSectionResizeMode(kColLine, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(kColPreview, QHeaderView::Stretch);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(6);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setAlternatingRowColors(false);

    m_previewDelegate = new ProjectSearchResultDelegate(m_tree);
    m_previewDelegate->setPreviewColumn(kColPreview);
    m_tree->setItemDelegateForColumn(kColPreview, m_previewDelegate);

    m_statusLabel = new QLabel(QString(), this);
    m_statusLabel->setObjectName(QStringLiteral("projectSearchStatus"));
    m_statusLabel->setWordWrap(false);
    m_statusLabel->setMinimumHeight(m_statusLabel->fontMetrics().height());

    m_searchBtn = new QPushButton(tr("Search"), this);
    m_searchBtn->setObjectName(QStringLiteral("projectSearchPrimary"));
    m_searchBtn->setDefault(true);
    m_searchBtn->setAutoDefault(true);

    auto *searchRow = new QHBoxLayout();
    searchRow->setSpacing(10);
    searchRow->addWidget(m_queryEdit, 1);
    searchRow->addWidget(m_searchBtn);

    auto *main = new QVBoxLayout(this);
    main->setSpacing(8);
    main->setContentsMargins(16, 16, 16, 16);
    main->addWidget(searchLabel);
    main->addLayout(searchRow);
    main->addWidget(m_statusLabel);
    main->addWidget(m_tree, 1);

    connect(m_searchBtn, &QPushButton::clicked, this, &ProjectSearchDialog::onSearchClicked);
    connect(m_tree, &QTreeWidget::itemClicked, this, &ProjectSearchDialog::onTreeItemActivated);
}

void ProjectSearchDialog::closeEvent(QCloseEvent *event)
{
    stopActiveSearch();
    QDialog::closeEvent(event);
}

void ProjectSearchDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    for (QTreeWidgetItem *it : m_fileNodes) {
        if (!it)
            continue;
        const QString p = it->data(kColLine, kPathRole).toString();
        if (!p.isEmpty())
            it->setText(kColLine, formatFileGroupLabel(p));
    }
}

QString ProjectSearchDialog::formatFileGroupLabel(const QString &absPath) const
{
    const QFileInfo fi(absPath);
    const QString fileName = fi.fileName();
    QString relDir = QDir(m_projectRoot).relativeFilePath(fi.absolutePath());
    relDir = QDir::fromNativeSeparators(relDir);
    if (relDir == QLatin1Char('.') || relDir.isEmpty())
        return fileName;

    const QFontMetrics fm(m_tree->font());
    const int avail = std::max(200, m_tree->viewport()->width() - 56);
    const QString sep = QStringLiteral("    ");
    const QString full = fileName + sep + relDir;
    if (fm.horizontalAdvance(full) <= avail)
        return full;

    const QString prefix = fileName + sep;
    const int reserved = fm.horizontalAdvance(prefix + QStringLiteral("…"));
    const int forDir = std::max(32, avail - reserved);
    const QString elidedDir = fm.elidedText(relDir, Qt::ElideMiddle, forDir);
    return prefix + elidedDir;
}

void ProjectSearchDialog::onSearchClicked()
{
    const QString q = m_queryEdit->text().trimmed();
    if (q.isEmpty()) {
        m_statusLabel->setText(tr("Enter a search string."));
        return;
    }

    stopActiveSearch();

    m_activeQuery = q;
    m_previewDelegate->setNeedle(m_activeQuery);
    m_matchCount = 0;
    m_fileNodes.clear();
    m_tree->clear();
    m_statusLabel->setText(tr("Searching…"));
    m_statusLabel->setVisible(true);
    m_searchBtn->setEnabled(false);

    m_thread = new QThread(this);
    m_worker = new ProjectSearchWorker();
    m_worker->moveToThread(m_thread);

    connect(m_worker, &ProjectSearchWorker::hitsBatch, this,
            &ProjectSearchDialog::onHitsBatch, Qt::QueuedConnection);
    connect(m_worker, &ProjectSearchWorker::searchFinished, this,
            &ProjectSearchDialog::onSearchFinished, Qt::QueuedConnection);
    connect(m_worker, &ProjectSearchWorker::searchFinished, m_thread, &QThread::quit,
            Qt::QueuedConnection);
    connect(m_worker, &ProjectSearchWorker::searchFinished, m_worker, &QObject::deleteLater,
            Qt::QueuedConnection);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    m_thread->start();
    QMetaObject::invokeMethod(m_worker, "runSearch", Qt::QueuedConnection,
                              Q_ARG(QString, m_projectRoot), Q_ARG(QString, q));
}

void ProjectSearchDialog::onHitsBatch(const QStringList &filePaths, const QList<int> &lineNumbers,
                                      const QStringList &linePreviews)
{
    m_tree->setUpdatesEnabled(false);
    for (int i = 0; i < filePaths.size(); ++i) {
        const QString &filePath = filePaths.at(i);
        const int lineNumber = lineNumbers.at(i);
        const QString &preview = linePreviews.at(i);

        const QString key = fileKey(filePath);
        QTreeWidgetItem *group = m_fileNodes.value(key);
        if (!group) {
            group = new QTreeWidgetItem(m_tree);
            group->setData(kColLine, kPathRole, filePath);
            group->setText(kColLine, formatFileGroupLabel(filePath));
            group->setFirstColumnSpanned(true);
            group->setExpanded(true);
            group->setForeground(kColLine, QColor(QStringLiteral("#9dc3e6")));
            m_fileNodes.insert(key, group);
        }

        auto *lineItem = new QTreeWidgetItem(group);
        lineItem->setText(kColLine, QString::number(lineNumber));
        lineItem->setData(kColLine, kLineRole, lineNumber);
        lineItem->setTextAlignment(kColLine, Qt::AlignLeft | Qt::AlignVCenter);
        lineItem->setText(kColPreview, preview);
        ++m_matchCount;
    }
    m_tree->setUpdatesEnabled(true);
}

void ProjectSearchDialog::onSearchFinished()
{
    m_searchBtn->setEnabled(true);
    const int nFiles = m_fileNodes.size();
    const QString resPhrase =
        (m_matchCount == 1) ? tr("1 result") : tr("%1 results").arg(m_matchCount);
    const QString filePhrase = (nFiles == 1) ? tr("1 file") : tr("%1 files").arg(nFiles);
    m_statusLabel->setText(tr("%1 in %2").arg(resPhrase).arg(filePhrase));
    m_thread = nullptr;
    m_worker = nullptr;
}

void ProjectSearchDialog::openResultIfLeaf(QTreeWidgetItem *item)
{
    if (!item || !item->parent())
        return;

    QTreeWidgetItem *group = item->parent();
    const QString path = group->data(kColLine, kPathRole).toString();
    const int line = item->data(kColLine, kLineRole).toInt();
    if (!path.isEmpty() && line > 0)
        emit openFileRequested(path, line, m_activeQuery.trimmed());
}

void ProjectSearchDialog::onTreeItemActivated(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    openResultIfLeaf(item);
}

void ProjectSearchDialog::stopActiveSearch()
{
    if (m_worker)
        m_worker->requestCancel();
    if (m_thread) {
        m_thread->wait(15000);
    }
    m_thread = nullptr;
    m_worker = nullptr;
}
