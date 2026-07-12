#include "SearchPanelWidget.h"
#include "SearchResultsWidget.h"
#include "ProjectSearchEngine.h"
#include "SearchDefs.h"
#include "libs/CodeEditor/include/widgets/CustomCodeEditor.h"
#include "app/IDEWindow/idewindow.h"
#include "ui/FilesTabWidget/filestabwidget.h"
#include "widgets/filetab.h"
#include "ui/ToolsTabWidget/toolstabwidget.h"
#include "core/modules/TabBase.h"

#include <QBoxLayout>
#include <QFormLayout>
#include <QKeyEvent>
#include <QApplication>
#include <QThread>
#include <QMessageBox>
#include <QFileInfo>
#include <QDirIterator>

enum SearchMode {
    ModeInFile = 0,
    ModeAcrossProject = 1
};

SearchPanelWidget::SearchPanelWidget(CustomCodeEditor* editor, QWidget* parent)
    : QWidget(parent)
    , m_editor(editor)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(4);

    // Mode selector
    auto* modeLayout = new QHBoxLayout();
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(6);

    auto* modeLabel = new QLabel(tr("Mode:"), this);
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("In File"));
    m_modeCombo->addItem(tr("Across Project"));
    m_modeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(m_modeCombo, 1);

    // Stacked content
    m_stack = new QStackedWidget(this);

    setupInFilePage();
    setupProjectPage();

    m_stack->addWidget(m_inFilePage);
    m_stack->addWidget(m_projectPage);

    rootLayout->addLayout(modeLayout);
    rootLayout->addWidget(m_stack, 1);

    // Timers
    m_inFileDebounce = new QTimer(this);
    m_inFileDebounce->setSingleShot(true);
    m_inFileDebounce->setInterval(300);

    m_projectDebounce = new QTimer(this);
    m_projectDebounce->setSingleShot(true);
    m_projectDebounce->setInterval(500);

    setupConnections();
}

void SearchPanelWidget::setEditor(CustomCodeEditor* editor)
{
    m_editor = editor;
    onInFileUpdateStatus();
}

void SearchPanelWidget::setupInFilePage()
{
    m_inFilePage = new QWidget(this);
    auto* layout = new QVBoxLayout(m_inFilePage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Search row
    auto* searchRow = new QHBoxLayout();
    searchRow->setSpacing(4);

    m_inFileSearchEdit = new QLineEdit(m_inFilePage);
    m_inFileSearchEdit->setPlaceholderText(tr("Search in file..."));
    m_inFileSearchEdit->setClearButtonEnabled(true);

    m_inFilePrevButton = new QPushButton(m_inFilePage);
    m_inFilePrevButton->setIcon(QIcon(":/icons/phoicons/icons-light/arrow-left-light.svg"));
    m_inFilePrevButton->setFixedSize(28, 28);
    m_inFilePrevButton->setToolTip(tr("Previous (Shift+F3)"));

    m_inFileNextButton = new QPushButton(m_inFilePage);
    m_inFileNextButton->setIcon(QIcon(":/icons/phoicons/icons-light/arrow-right-light.svg"));
    m_inFileNextButton->setFixedSize(28, 28);
    m_inFileNextButton->setToolTip(tr("Next (F3)"));

    m_inFileStatusLabel = new QLabel("0 / 0", m_inFilePage);
    m_inFileStatusLabel->setMinimumWidth(50);

    searchRow->addWidget(m_inFileSearchEdit, 1);
    searchRow->addWidget(m_inFilePrevButton);
    searchRow->addWidget(m_inFileNextButton);
    searchRow->addWidget(m_inFileStatusLabel);

    // Replace row (always visible)
    auto* replaceRow = new QHBoxLayout();
    replaceRow->setSpacing(4);

    m_inFileReplaceEdit = new QLineEdit(m_inFilePage);
    m_inFileReplaceEdit->setPlaceholderText(tr("Replace with..."));
    m_inFileReplaceEdit->setClearButtonEnabled(true);

    m_inFileReplaceButton = new QPushButton(tr("Replace"), m_inFilePage);
    m_inFileReplaceAllButton = new QPushButton(tr("All"), m_inFilePage);

    replaceRow->addWidget(m_inFileReplaceEdit, 1);
    replaceRow->addWidget(m_inFileReplaceButton);
    replaceRow->addWidget(m_inFileReplaceAllButton);

    // Options row
    auto* optionsRow = new QHBoxLayout();
    optionsRow->setSpacing(12);

    m_inFileMatchCase = new QCheckBox(tr("Case"), m_inFilePage);
    m_inFileRegex = new QCheckBox(tr("Regex"), m_inFilePage);
    m_inFileWholeWord = new QCheckBox(tr("Word"), m_inFilePage);
    m_inFileWholeWord->setChecked(true);

    optionsRow->addWidget(m_inFileMatchCase);
    optionsRow->addWidget(m_inFileRegex);
    optionsRow->addWidget(m_inFileWholeWord);
    optionsRow->addStretch();

    layout->addLayout(searchRow);
    layout->addLayout(replaceRow);
    layout->addLayout(optionsRow);
    layout->addStretch();
}

void SearchPanelWidget::setupProjectPage()
{
    m_projectPage = new QWidget(this);
    auto* layout = new QVBoxLayout(m_projectPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Search row
    auto* searchRow = new QHBoxLayout();
    searchRow->setSpacing(4);

    m_projectSearchEdit = new QLineEdit(m_projectPage);
    m_projectSearchEdit->setPlaceholderText(tr("Search across project..."));
    m_projectSearchEdit->setClearButtonEnabled(true);

    m_projectSearchButton = new QPushButton(tr("Search"), m_projectPage);

    m_projectStatusLabel = new QLabel(m_projectPage);
    m_projectStatusLabel->setMinimumWidth(80);

    searchRow->addWidget(m_projectSearchEdit, 1);
    searchRow->addWidget(m_projectSearchButton);
    searchRow->addWidget(m_projectStatusLabel);

    // Replace row (always visible)
    auto* replaceRow = new QHBoxLayout();
    replaceRow->setSpacing(4);

    m_projectReplaceEdit = new QLineEdit(m_projectPage);
    m_projectReplaceEdit->setPlaceholderText(tr("Replace with..."));
    m_projectReplaceEdit->setClearButtonEnabled(true);

    m_projectReplaceAllButton = new QPushButton(tr("Replace All"), m_projectPage);

    replaceRow->addWidget(m_projectReplaceEdit, 1);
    replaceRow->addWidget(m_projectReplaceAllButton);

    // Options row
    auto* optionsRow = new QHBoxLayout();
    optionsRow->setSpacing(8);

    m_projectMatchCase = new QCheckBox(tr("Case"), m_projectPage);
    m_projectRegex = new QCheckBox(tr("Regex"), m_projectPage);
    m_projectWholeWord = new QCheckBox(tr("Word"), m_projectPage);
    m_projectOpenFilesOnly = new QCheckBox(tr("Open only"), m_projectPage);

    optionsRow->addWidget(m_projectMatchCase);
    optionsRow->addWidget(m_projectRegex);
    optionsRow->addWidget(m_projectWholeWord);
    optionsRow->addWidget(m_projectOpenFilesOnly);
    optionsRow->addStretch();

    // Progress bar
    m_projectProgressBar = new QProgressBar(m_projectPage);
    m_projectProgressBar->setRange(0, 0);
    m_projectProgressBar->setFixedHeight(2);
    m_projectProgressBar->setTextVisible(false);
    m_projectProgressBar->hide();

    // Results container
    m_projectResultsContainer = new QWidget(m_projectPage);
    auto* resultsLayout = new QVBoxLayout(m_projectResultsContainer);
    resultsLayout->setContentsMargins(0, 0, 0, 0);

    layout->addLayout(searchRow);
    layout->addLayout(replaceRow);
    layout->addLayout(optionsRow);
    layout->addWidget(m_projectProgressBar);
    layout->addWidget(m_projectResultsContainer, 1);
}

void SearchPanelWidget::setupConnections()
{
    // Mode switch
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SearchPanelWidget::onModeChanged);

    // In File connections
    connect(m_inFileSearchEdit, &QLineEdit::textChanged,
            this, &SearchPanelWidget::onInFileSearchTextChanged);
    connect(m_inFileSearchEdit, &QLineEdit::returnPressed, this, &SearchPanelWidget::findNext);
    connect(m_inFilePrevButton, &QPushButton::clicked, this, &SearchPanelWidget::findPrevious);
    connect(m_inFileNextButton, &QPushButton::clicked, this, &SearchPanelWidget::findNext);
    connect(m_inFileReplaceButton, &QPushButton::clicked, this, &SearchPanelWidget::onInFileReplaceCurrent);
    connect(m_inFileReplaceAllButton, &QPushButton::clicked, this, &SearchPanelWidget::onInFileReplaceAll);
    connect(m_inFileMatchCase, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState) { onInFileUpdateStatus(); });
    connect(m_inFileRegex, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState) { onInFileUpdateStatus(); });
    connect(m_inFileWholeWord, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState) { onInFileUpdateStatus(); });

    // Live search debounce - In File
    connect(m_inFileDebounce, &QTimer::timeout, this, &SearchPanelWidget::onInFileDebounceTimeout);

    // Project connections
    connect(m_projectSearchEdit, &QLineEdit::textChanged,
            this, &SearchPanelWidget::onProjectSearchTextChanged);
    connect(m_projectSearchEdit, &QLineEdit::returnPressed, this, &SearchPanelWidget::onProjectSearch);
    connect(m_projectSearchButton, &QPushButton::clicked, this, &SearchPanelWidget::onProjectSearch);
    connect(m_projectReplaceAllButton, &QPushButton::clicked, this, &SearchPanelWidget::onProjectReplaceAll);

    // Live search debounce - Project
    connect(m_projectDebounce, &QTimer::timeout, this, &SearchPanelWidget::onProjectDebounceTimeout);
}

void SearchPanelWidget::onModeChanged(int index)
{
    m_stack->setCurrentIndex(index);
    if (index == ModeInFile) {
        m_inFileSearchEdit->setFocus();
    } else {
        m_projectSearchEdit->setFocus();
    }
}

void SearchPanelWidget::showInFileMode()
{
    m_modeCombo->setCurrentIndex(ModeInFile);
    m_inFileSearchEdit->setFocus();
    m_inFileSearchEdit->selectAll();
    onInFileUpdateStatus();
}

void SearchPanelWidget::showProjectMode()
{
    m_modeCombo->setCurrentIndex(ModeAcrossProject);
    m_projectSearchEdit->setFocus();
    m_projectSearchEdit->selectAll();
}

void SearchPanelWidget::findNext()
{
    if (m_inFileSearchEdit->text().isEmpty() || !m_editor)
        return;

    Qt::CaseSensitivity cs = m_inFileMatchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    bool ww = m_inFileWholeWord->isChecked();
    m_editor->findText(m_inFileSearchEdit->text(), true, cs, ww);
    onInFileUpdateStatus();
}

void SearchPanelWidget::findPrevious()
{
    if (m_inFileSearchEdit->text().isEmpty() || !m_editor)
        return;

    Qt::CaseSensitivity cs = m_inFileMatchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    bool ww = m_inFileWholeWord->isChecked();
    m_editor->findText(m_inFileSearchEdit->text(), false, cs, ww);
    onInFileUpdateStatus();
}

void SearchPanelWidget::onInFileSearchTextChanged(const QString& text)
{
    Q_UNUSED(text);
    m_inFileDebounce->start();
}

void SearchPanelWidget::onInFileDebounceTimeout()
{
    onInFileUpdateStatus();

    // Auto-find first match when text is not empty
    if (!m_inFileSearchEdit->text().isEmpty() && m_editor) {
        Qt::CaseSensitivity cs = m_inFileMatchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
        bool ww = m_inFileWholeWord->isChecked();
        m_editor->findText(m_inFileSearchEdit->text(), true, cs, ww);
        onInFileUpdateStatus();
    }
}

void SearchPanelWidget::onInFileReplaceCurrent()
{
    if (!m_editor || m_inFileSearchEdit->text().isEmpty())
        return;

    Qt::CaseSensitivity cs = m_inFileMatchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    bool ww = m_inFileWholeWord->isChecked();
    const QString search = m_inFileSearchEdit->text();
    const QString replace = m_inFileReplaceEdit->text();

    // If nothing selected or selection doesn't match, find first
    if (!m_editor->hasSelection() || m_editor->selectedText().compare(search, cs) != 0) {
        if (!m_editor->findText(search, true, cs, ww))
            return;
    }

    // Now replace the current selection
    m_editor->replaceCurrentSelection(search, replace, cs, ww);

    // Find next occurrence
    m_editor->findText(search, true, cs, ww);
    onInFileUpdateStatus();
}

void SearchPanelWidget::onInFileReplaceAll()
{
    if (!m_editor || m_inFileSearchEdit->text().isEmpty())
        return;

    Qt::CaseSensitivity cs = m_inFileMatchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    bool ww = m_inFileWholeWord->isChecked();
    m_editor->replaceAllMatches(m_inFileSearchEdit->text(), m_inFileReplaceEdit->text(), cs, ww);
    onInFileUpdateStatus();
}

void SearchPanelWidget::onInFileUpdateStatus()
{
    if (!m_editor) {
        m_inFileStatusLabel->setText("0 / 0");
        return;
    }

    Qt::CaseSensitivity cs = m_inFileMatchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    bool ww = m_inFileWholeWord->isChecked();
    const int total = m_editor->countMatches(m_inFileSearchEdit->text(), cs, ww);
    const int current = m_editor->currentMatchIndex(m_inFileSearchEdit->text(), cs, ww);
    m_inFileStatusLabel->setText(QString("%1 / %2").arg(current).arg(total));

    const bool hasQuery = !m_inFileSearchEdit->text().isEmpty();
    m_inFilePrevButton->setEnabled(hasQuery && total > 0);
    m_inFileNextButton->setEnabled(hasQuery && total > 0);
    m_inFileReplaceButton->setEnabled(hasQuery && total > 0);
    m_inFileReplaceAllButton->setEnabled(hasQuery && total > 0);
}

void SearchPanelWidget::onProjectSearchTextChanged(const QString& text)
{
    Q_UNUSED(text);
    if (m_projectSearchEdit->text().trimmed().length() >= 2) {
        m_projectDebounce->start();
    }
}

void SearchPanelWidget::onProjectDebounceTimeout()
{
    onProjectSearch();
}

void SearchPanelWidget::onProjectSearch()
{
    const QString query = m_projectSearchEdit->text().trimmed();
    if (query.isEmpty())
        return;

    // Clean up previous results
    QLayout* layout = m_projectResultsContainer->layout();
    if (layout) {
        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    }

    m_projectProgressBar->show();
    m_projectStatusLabel->setText(tr("Searching..."));

    QString projectPath = findProjectPath();
    if (projectPath.isEmpty()) {
        m_projectProgressBar->hide();
        m_projectStatusLabel->setText(tr("No project"));
        return;
    }

    auto* resultsWidget = new SearchResultsWidget(m_projectResultsContainer);
    layout->addWidget(resultsWidget);

    connect(resultsWidget, &SearchResultsWidget::resultActivated,
            this, &SearchPanelWidget::onProjectResultActivated);

    auto* engine = new ProjectSearchEngine(this);
    auto* thread = new QThread(this);

    engine->moveToThread(thread);

    bool caseSensitive = m_projectMatchCase->isChecked();
    bool useRegex = m_projectRegex->isChecked();
    bool wholeWord = m_projectWholeWord->isChecked();
    bool openFilesOnly = m_projectOpenFilesOnly->isChecked();
    QStringList openFiles = openFilePaths();

    connect(thread, &QThread::started, engine, [engine, projectPath, query, caseSensitive, useRegex, wholeWord, openFilesOnly, openFiles]() {
        engine->setSearchParams(projectPath, query, caseSensitive, useRegex, wholeWord, openFilesOnly, openFiles);
        engine->execute();
    });

    connect(engine, &ProjectSearchEngine::resultFound, resultsWidget, &SearchResultsWidget::addSingleResult);

    connect(engine, &ProjectSearchEngine::searchFinished, this, [this, engine, thread, resultsWidget](int totalMatches, int totalFiles) {
        m_projectProgressBar->hide();
        if (totalMatches == 0)
            m_projectStatusLabel->setText(tr("No results"));
        else
            m_projectStatusLabel->setText(tr("%1 in %2 files").arg(totalMatches).arg(totalFiles));
        engine->deleteLater();
        thread->quit();
        thread->wait();
        thread->deleteLater();
    });

    thread->start();
}

void SearchPanelWidget::onProjectReplaceCurrent()
{
    // Not applicable for project search - use Replace All instead
}

void SearchPanelWidget::onProjectReplaceAll()
{
    const QString query = m_projectSearchEdit->text().trimmed();
    const QString replacement = m_projectReplaceEdit->text();
    if (query.isEmpty())
        return;

    SearchResultsWidget* resultsWidget = nullptr;
    QLayout* layout = m_projectResultsContainer->layout();
    if (layout && layout->count() > 0) {
        resultsWidget = qobject_cast<SearchResultsWidget*>(layout->itemAt(0)->widget());
    }

    if (!resultsWidget || resultsWidget->totalMatches() == 0) {
        QMessageBox::information(this, tr("Replace All"), tr("No results to replace. Run a search first."));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Replace All in Project"),
        tr("Replace %1 occurrences across %2 files?").arg(resultsWidget->totalMatches()).arg(resultsWidget->totalFileCount()),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes)
        return;

    bool caseSensitive = m_projectMatchCase->isChecked();
    bool useRegex = m_projectRegex->isChecked();
    bool wholeWord = m_projectWholeWord->isChecked();

    auto* engine = new ProjectSearchEngine(this);
    engine->replaceAllInProject(findProjectPath(), query, replacement, caseSensitive, useRegex, wholeWord);

    QMessageBox::information(this, tr("Replace All"), tr("Replacement complete."));

    engine->deleteLater();

    onProjectSearch();
}

void SearchPanelWidget::onProjectResultActivated(const SearchResult& result)
{
    QFileInfo fi(result.filePath);
    emit openFileAtLine(result.filePath, result.lineNumber, result.lineText.mid(result.matchStart, result.matchLength));
}

QString SearchPanelWidget::findProjectPath() const
{
    QWidget* w = parentWidget();
    while (w) {
        if (auto* ideWin = qobject_cast<IDEWindow*>(w))
            return ideWin->projectPath();
        w = w->parentWidget();
    }
    return {};
}

QStringList SearchPanelWidget::openFilePaths() const
{
    QStringList paths;

    QWidget* w = parentWidget();
    while (w) {
        if (auto* ideWin = qobject_cast<IDEWindow*>(w)) {
            FilesTabWidget* tabWidget = ideWin->findChild<FilesTabWidget*>();
            if (tabWidget) {
                for (int i = 0; i < tabWidget->count(); ++i) {
                    FileTab* tab = qobject_cast<FileTab*>(tabWidget->widget(i));
                    if (tab)
                        paths.append(tab->filePath);
                }
            }
            break;
        }
        w = w->parentWidget();
    }

    return paths;
}
