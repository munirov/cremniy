#include "findpanel.h"

#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr int MatchIndexRole = Qt::UserRole + 1;

QString scopeName(SearchScope scope)
{
    switch (scope) {
    case SearchScope::CurrentFile:
        return FindPanel::tr("Current file");
    case SearchScope::OpenFiles:
        return FindPanel::tr("Open files");
    case SearchScope::Project:
        return FindPanel::tr("Entire project");
    }
    return {};
}

} // namespace

FindPanel::FindPanel(const QString& projectPath, QWidget* parent)
    : QWidget(parent)
    , m_projectPath(QDir::cleanPath(projectPath))
{
    setObjectName(QStringLiteral("searchPanel"));
    setMinimumWidth(340);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    buildUi();

    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(220);
    connect(m_searchTimer, &QTimer::timeout, this, [this] {
        emit searchRequested(currentOptions());
    });

    connect(m_searchEdit, &QLineEdit::textChanged, this, &FindPanel::scheduleSearch);
    connect(m_replaceEdit, &QLineEdit::textChanged, this, [this] {
        if (m_regexButton->isChecked())
            scheduleSearch();
    });
    connect(m_scopeCombo, &QComboBox::currentIndexChanged, this, [this] {
        const auto scope = static_cast<SearchScope>(m_scopeCombo->currentData().toInt());
        m_filterRow->setVisible(scope == SearchScope::Project);
        scheduleSearch();
    });
    connect(m_matchCaseButton, &QToolButton::toggled, this, &FindPanel::scheduleSearch);
    connect(m_wholeWordButton, &QToolButton::toggled, this, &FindPanel::scheduleSearch);
    connect(m_regexButton, &QToolButton::toggled, this, &FindPanel::scheduleSearch);
    connect(m_includeEdit, &QLineEdit::textChanged, this, &FindPanel::scheduleSearch);
    connect(m_excludeEdit, &QLineEdit::textChanged, this, &FindPanel::scheduleSearch);
    connect(m_replaceToggle, &QToolButton::toggled, this, &FindPanel::setReplaceMode);
    connect(m_previousButton, &QPushButton::clicked, this, [this] { selectAdjacentResult(false); });
    connect(m_nextButton, &QPushButton::clicked, this, [this] { selectAdjacentResult(true); });
    connect(m_replaceButton, &QPushButton::clicked, this, &FindPanel::replaceCurrentResult);
    connect(m_replaceAllButton, &QPushButton::clicked, this, &FindPanel::replaceAllResults);
    connect(m_resultsTree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem*, int) {
        activateCurrentResult();
    });
    connect(m_resultsTree, &QTreeWidget::itemSelectionChanged, this, [this] {
        m_replaceButton->setEnabled(m_replaceMode && selectedMatchIndex() >= 0);
    });
    auto* nextShortcut = new QShortcut(QKeySequence(Qt::Key_F3), this);
    nextShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(nextShortcut, &QShortcut::activated, this, [this] { selectAdjacentResult(true); });
    auto* previousShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3), this);
    previousShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(previousShortcut, &QShortcut::activated, this, [this] { selectAdjacentResult(false); });

    installEventFilter(this);
    for (QWidget* child : findChildren<QWidget*>())
        child->installEventFilter(this);

    showEmptyState(tr("Search this project"), tr("Enter text above to find matches."));
}

void FindPanel::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 8, 10, 8);
    rootLayout->setSpacing(7);

    auto* searchRow = new QHBoxLayout();
    searchRow->setSpacing(6);

    m_replaceToggle = new QToolButton(this);
    m_replaceToggle->setText(QStringLiteral("›"));
    m_replaceToggle->setCheckable(true);
    m_replaceToggle->setToolTip(tr("Show replace controls (Ctrl+H)"));
    m_replaceToggle->setAccessibleName(tr("Show replace controls"));
    m_replaceToggle->setObjectName(QStringLiteral("searchDisclosureButton"));

    auto* searchLabel = new QLabel(tr("&Search"), this);
    searchLabel->setObjectName(QStringLiteral("searchFieldLabel"));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("searchQueryEdit"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(tr("Text or regular expression"));
    m_searchEdit->setAccessibleName(tr("Search text"));
    searchLabel->setBuddy(m_searchEdit);

    m_scopeCombo = new QComboBox(this);
    m_scopeCombo->setObjectName(QStringLiteral("searchScopeCombo"));
    m_scopeCombo->addItem(scopeName(SearchScope::CurrentFile), static_cast<int>(SearchScope::CurrentFile));
    m_scopeCombo->addItem(scopeName(SearchScope::OpenFiles), static_cast<int>(SearchScope::OpenFiles));
    m_scopeCombo->addItem(scopeName(SearchScope::Project), static_cast<int>(SearchScope::Project));
    m_scopeCombo->setAccessibleName(tr("Search scope"));

    auto makeOptionButton = [this](const QString& text, const QString& name, const QString& tooltip) {
        auto* button = new QToolButton(this);
        button->setText(text);
        button->setCheckable(true);
        button->setObjectName(QStringLiteral("searchOptionButton"));
        button->setAccessibleName(name);
        button->setToolTip(tooltip);
        button->setMinimumSize(32, 30);
        return button;
    };
    m_matchCaseButton = makeOptionButton(QStringLiteral("Aa"), tr("Match case"), tr("Match case"));
    m_wholeWordButton = makeOptionButton(QStringLiteral("W"), tr("Match whole word"), tr("Match whole word"));
    m_regexButton = makeOptionButton(QStringLiteral(".*"), tr("Use regular expression"), tr("Use regular expression"));

    m_previousButton = new QPushButton(tr("Previous"), this);
    m_previousButton->setToolTip(tr("Previous result (Shift+F3)"));
    m_nextButton = new QPushButton(tr("Next"), this);
    m_nextButton->setToolTip(tr("Next result (F3)"));

    auto* closeButton = new QToolButton(this);
    closeButton->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
    closeButton->setToolTip(tr("Close search (Esc)"));
    closeButton->setAccessibleName(tr("Close search"));
    closeButton->setObjectName(QStringLiteral("searchCloseButton"));
    closeButton->setMinimumSize(32, 30);
    connect(closeButton, &QToolButton::clicked, this, &FindPanel::closeRequested);

    searchRow->addWidget(m_replaceToggle);
    searchRow->addWidget(searchLabel);
    searchRow->addWidget(m_searchEdit, 1);
    searchRow->addWidget(closeButton);
    rootLayout->addLayout(searchRow);

    auto* optionsRow = new QHBoxLayout();
    optionsRow->setContentsMargins(38, 0, 38, 0);
    optionsRow->setSpacing(6);
    optionsRow->addWidget(m_scopeCombo, 1);
    optionsRow->addWidget(m_matchCaseButton);
    optionsRow->addWidget(m_wholeWordButton);
    optionsRow->addWidget(m_regexButton);
    rootLayout->addLayout(optionsRow);

    auto* navigationRow = new QHBoxLayout();
    navigationRow->setContentsMargins(38, 0, 38, 0);
    navigationRow->setSpacing(6);
    navigationRow->addWidget(m_previousButton, 1);
    navigationRow->addWidget(m_nextButton, 1);
    rootLayout->addLayout(navigationRow);

    m_replaceRow = new QWidget(this);
    auto* replaceLayout = new QVBoxLayout(m_replaceRow);
    replaceLayout->setContentsMargins(38, 0, 38, 0);
    replaceLayout->setSpacing(6);
    auto* replaceFieldRow = new QHBoxLayout();
    replaceFieldRow->setSpacing(6);
    auto* replaceLabel = new QLabel(tr("&Replace"), m_replaceRow);
    m_replaceEdit = new QLineEdit(m_replaceRow);
    m_replaceEdit->setObjectName(QStringLiteral("searchReplacementEdit"));
    m_replaceEdit->setClearButtonEnabled(true);
    m_replaceEdit->setPlaceholderText(tr("Replacement text"));
    m_replaceEdit->setAccessibleName(tr("Replacement text"));
    replaceLabel->setBuddy(m_replaceEdit);
    m_replaceButton = new QPushButton(tr("Replace result"), m_replaceRow);
    m_replaceAllButton = new QPushButton(tr("Replace all"), m_replaceRow);
    replaceFieldRow->addWidget(replaceLabel);
    replaceFieldRow->addWidget(m_replaceEdit, 1);
    auto* replaceActionsRow = new QHBoxLayout();
    replaceActionsRow->setSpacing(6);
    replaceActionsRow->addWidget(m_replaceButton, 1);
    replaceActionsRow->addWidget(m_replaceAllButton, 1);
    replaceLayout->addLayout(replaceFieldRow);
    replaceLayout->addLayout(replaceActionsRow);
    rootLayout->addWidget(m_replaceRow);

    m_filterRow = new QWidget(this);
    auto* filterLayout = new QFormLayout(m_filterRow);
    filterLayout->setContentsMargins(38, 0, 38, 0);
    filterLayout->setHorizontalSpacing(6);
    filterLayout->setVerticalSpacing(6);
    filterLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto* includeLabel = new QLabel(tr("&Include"), m_filterRow);
    m_includeEdit = new QLineEdit(m_filterRow);
    m_includeEdit->setObjectName(QStringLiteral("searchIncludeEdit"));
    m_includeEdit->setPlaceholderText(tr("For example: *.cpp, src/*"));
    m_includeEdit->setAccessibleName(tr("Files to include"));
    includeLabel->setBuddy(m_includeEdit);
    auto* excludeLabel = new QLabel(tr("E&xclude"), m_filterRow);
    m_excludeEdit = new QLineEdit(m_filterRow);
    m_excludeEdit->setObjectName(QStringLiteral("searchExcludeEdit"));
    m_excludeEdit->setPlaceholderText(tr("For example: build, *.min.js"));
    m_excludeEdit->setAccessibleName(tr("Files to exclude"));
    excludeLabel->setBuddy(m_excludeEdit);
    filterLayout->addRow(includeLabel, m_includeEdit);
    filterLayout->addRow(excludeLabel, m_excludeEdit);
    rootLayout->addWidget(m_filterRow);

    m_resultsStack = new QStackedWidget(this);
    m_resultsTree = new QTreeWidget(m_resultsStack);
    m_resultsTree->setObjectName(QStringLiteral("searchResultsTree"));
    m_resultsTree->setColumnCount(2);
    m_resultsTree->setHeaderLabels({tr("Result"), tr("Location")});
    m_resultsTree->setRootIsDecorated(true);
    m_resultsTree->setUniformRowHeights(true);
    m_resultsTree->setAlternatingRowColors(true);
    m_resultsTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultsTree->setAccessibleName(tr("Search results"));
    m_resultsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultsTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    auto* emptyWidget = new QWidget(m_resultsStack);
    auto* emptyLayout = new QVBoxLayout(emptyWidget);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(3);
    m_emptyTitle = new QLabel(emptyWidget);
    m_emptyTitle->setObjectName(QStringLiteral("searchEmptyTitle"));
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    m_emptyDetail = new QLabel(emptyWidget);
    m_emptyDetail->setObjectName(QStringLiteral("searchEmptyDetail"));
    m_emptyDetail->setAlignment(Qt::AlignCenter);
    m_emptyDetail->setWordWrap(true);
    emptyLayout->addWidget(m_emptyTitle);
    emptyLayout->addWidget(m_emptyDetail);

    m_resultsStack->addWidget(m_resultsTree);
    m_resultsStack->addWidget(emptyWidget);
    rootLayout->addWidget(m_resultsStack, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("searchStatusLabel"));
    m_statusLabel->setAccessibleName(tr("Search status"));
    m_statusLabel->setWordWrap(true);
    rootLayout->addWidget(m_statusLabel);

    m_filterRow->hide();
    setReplaceMode(false);
}

void FindPanel::open(SearchScope scope, bool replaceMode, const QString& initialQuery)
{
    const int scopeIndex = m_scopeCombo->findData(static_cast<int>(scope));
    if (scopeIndex >= 0)
        m_scopeCombo->setCurrentIndex(scopeIndex);
    m_replaceToggle->setChecked(replaceMode);
    setReplaceMode(replaceMode);

    if (!initialQuery.isEmpty())
        m_searchEdit->setText(initialQuery);
    m_searchEdit->setFocus(Qt::ShortcutFocusReason);
    if (initialQuery.isEmpty())
        m_searchEdit->selectAll();
    scheduleSearch();
}

void FindPanel::nextResult()
{
    selectAdjacentResult(true);
}

void FindPanel::previousResult()
{
    selectAdjacentResult(false);
}

void FindPanel::refresh()
{
    scheduleSearch();
}

bool FindPanel::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched);
    if (event->type() == QEvent::KeyPress) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            emit closeRequested();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FindPanel::scheduleSearch()
{
    emit searchInvalidated();
    m_searchTimer->start();
}

SearchOptions FindPanel::currentOptions() const
{
    SearchOptions options;
    options.query = m_searchEdit->text();
    options.replacement = m_replaceEdit->text();
    options.scope = static_cast<SearchScope>(m_scopeCombo->currentData().toInt());
    options.caseSensitive = m_matchCaseButton->isChecked();
    options.wholeWord = m_wholeWordButton->isChecked();
    options.regularExpression = m_regexButton->isChecked();
    options.includePattern = m_includeEdit->text();
    options.excludePattern = m_excludeEdit->text();
    return options;
}

void FindPanel::setReplaceMode(bool enabled)
{
    m_replaceMode = enabled;
    m_replaceRow->setVisible(enabled);
    m_replaceToggle->setText(enabled ? QStringLiteral("⌄") : QStringLiteral("›"));
    m_replaceButton->setEnabled(enabled && selectedMatchIndex() >= 0);
    m_replaceAllButton->setEnabled(enabled && !m_report.matches.isEmpty() && !m_report.truncated);
}

void FindPanel::setSearching(bool searching)
{
    m_statusLabel->setProperty("statusState", searching ? "busy" : "normal");
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
    if (searching)
        m_statusLabel->setText(tr("Searching…"));
    m_previousButton->setEnabled(!searching && !m_report.matches.isEmpty());
    m_nextButton->setEnabled(!searching && !m_report.matches.isEmpty());
    m_replaceButton->setEnabled(!searching && m_replaceMode && selectedMatchIndex() >= 0);
    m_replaceAllButton->setEnabled(!searching && m_replaceMode && !m_report.matches.isEmpty() && !m_report.truncated);
}

void FindPanel::showReport(const SearchReport& report)
{
    m_report = report;
    m_resultsTree->clear();

    if (!report.error.isEmpty()) {
        m_searchEdit->setProperty("state", "error");
        m_searchEdit->style()->unpolish(m_searchEdit);
        m_searchEdit->style()->polish(m_searchEdit);
        showEmptyState(tr("Unable to search"), report.error);
        m_statusLabel->setProperty("statusState", "error");
        m_statusLabel->setText(report.error);
        return;
    }

    m_searchEdit->setProperty("state", QVariant());
    m_searchEdit->style()->unpolish(m_searchEdit);
    m_searchEdit->style()->polish(m_searchEdit);

    if (report.matches.isEmpty()) {
        const QString detail = report.filesSearched == 0
            ? tr("Open a text file or choose another scope.")
            : tr("Try another query or adjust the file filters.");
        showEmptyState(tr("No matches found"), detail);
    } else {
        QHash<QString, QTreeWidgetItem*> fileItems;
        QHash<QString, int> fileCounts;
        for (int index = 0; index < report.matches.size(); ++index) {
            const SearchMatch& match = report.matches.at(index);
            QTreeWidgetItem* fileItem = fileItems.value(match.filePath, nullptr);
            if (!fileItem) {
                fileItem = new QTreeWidgetItem(m_resultsTree);
                fileItem->setText(0, displayPath(match.filePath));
                fileItem->setToolTip(0, match.filePath);
                QFont font = fileItem->font(0);
                font.setBold(true);
                fileItem->setFont(0, font);
                fileItems.insert(match.filePath, fileItem);
            }

            ++fileCounts[match.filePath];
            auto* resultItem = new QTreeWidgetItem(fileItem);
            resultItem->setText(0, match.preview.isEmpty() ? tr("Empty line") : match.preview);
            resultItem->setText(1, tr("Line %1, column %2").arg(match.line).arg(match.column + 1));
            resultItem->setData(0, MatchIndexRole, index);
            resultItem->setToolTip(0, match.preview);
        }
        for (auto it = fileItems.cbegin(); it != fileItems.cend(); ++it)
            it.value()->setText(1, tr("%n match(es)", nullptr, fileCounts.value(it.key())));

        m_resultsTree->expandAll();
        m_resultsStack->setCurrentWidget(m_resultsTree);
        if (m_resultsTree->topLevelItemCount() > 0 && m_resultsTree->topLevelItem(0)->childCount() > 0) {
            QTreeWidgetItem* first = m_resultsTree->topLevelItem(0)->child(0);
            m_resultsTree->setCurrentItem(first);
            first->setSelected(true);
        }
    }

    QString status = tr("%n match(es)", nullptr, report.matches.size())
        + tr(" in %n file(s)", nullptr, report.filesSearched);
    if (report.filesSkipped > 0)
        status += tr(" · %n binary, unreadable, or large file(s) skipped", nullptr, report.filesSkipped);
    if (report.truncated)
        status += tr(" · Result limit reached");
    m_statusLabel->setProperty("statusState", report.truncated ? "warning" : "normal");
    m_statusLabel->setText(status);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
    m_previousButton->setEnabled(!report.matches.isEmpty());
    m_nextButton->setEnabled(!report.matches.isEmpty());
    m_replaceButton->setEnabled(m_replaceMode && selectedMatchIndex() >= 0);
    m_replaceAllButton->setEnabled(m_replaceMode && !report.matches.isEmpty() && !report.truncated);
    m_replaceAllButton->setToolTip(report.truncated
        ? tr("Refine the search before replacing all matches.")
        : QString());
    emit statusMessage(status);
}

void FindPanel::setReport(const SearchReport& report)
{
    setSearching(false);
    showReport(report);
}

void FindPanel::showEmptyState(const QString& title, const QString& detail)
{
    m_emptyTitle->setText(title);
    m_emptyDetail->setText(detail);
    m_emptyDetail->setVisible(!detail.isEmpty());
    m_resultsStack->setCurrentIndex(1);
    m_previousButton->setEnabled(false);
    m_nextButton->setEnabled(false);
    m_replaceButton->setEnabled(false);
    m_replaceAllButton->setEnabled(false);
}

int FindPanel::selectedMatchIndex() const
{
    QTreeWidgetItem* item = m_resultsTree->currentItem();
    if (!item)
        return -1;
    bool ok = false;
    const int index = item->data(0, MatchIndexRole).toInt(&ok);
    return ok && index >= 0 && index < m_report.matches.size() ? index : -1;
}

void FindPanel::activateCurrentResult()
{
    const int index = selectedMatchIndex();
    if (index < 0)
        return;
    emit resultActivated(m_report.matches.at(index));
}

void FindPanel::selectAdjacentResult(bool forward)
{
    if (m_report.matches.isEmpty())
        return;

    int index = selectedMatchIndex();
    if (index < 0)
        index = forward ? 0 : m_report.matches.size() - 1;
    else
        index = (index + (forward ? 1 : -1) + m_report.matches.size()) % m_report.matches.size();

    QTreeWidgetItemIterator iterator(m_resultsTree);
    while (*iterator) {
        bool isMatch = false;
        const int itemIndex = (*iterator)->data(0, MatchIndexRole).toInt(&isMatch);
        if (isMatch && itemIndex == index) {
            m_resultsTree->setCurrentItem(*iterator);
            m_resultsTree->scrollToItem(*iterator);
            activateCurrentResult();
            break;
        }
        ++iterator;
    }
}

void FindPanel::replaceCurrentResult()
{
    const int index = selectedMatchIndex();
    if (index < 0)
        return;

    emit replaceRequested(m_report.matches.at(index), currentOptions());
}

void FindPanel::replaceAllResults()
{
    if (m_report.matches.isEmpty())
        return;

    QSet<QString> filePaths;
    for (const SearchMatch& match : std::as_const(m_report.matches))
        filePaths.insert(match.filePath);

    QMessageBox confirmation(QMessageBox::Question,
                             tr("Replace all matches"),
                             tr("Replace %n match(es)", nullptr, m_report.matches.size())
                                 + tr(" in %n file(s)?", nullptr, filePaths.size()),
                             QMessageBox::NoButton,
                             this);
    auto* replaceButton = confirmation.addButton(tr("Replace all"), QMessageBox::AcceptRole);
    confirmation.addButton(tr("Cancel"), QMessageBox::RejectRole);
    confirmation.exec();
    if (confirmation.clickedButton() != replaceButton)
        return;

    emit replaceAllRequested(m_report, currentOptions());
}

QString FindPanel::displayPath(const QString& filePath) const
{
    QString relative = QDir(m_projectPath).relativeFilePath(filePath);
    return relative.startsWith(QStringLiteral("..")) ? QFileInfo(filePath).fileName() : relative.replace('\\', '/');
}
