#include "codeeditortab.h"
#include "core/ToolTabFactory.h"
#include "filemanager.h"
#include "utils.h"

#include <QBoxLayout>
#include <QFileInfo>
#include <QInputDialog>
#include <QtGlobal>
#include <QLabel>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QPushButton>
#include <QShortcut>
#include <QStackedLayout>
#include <QVBoxLayout>

static bool registered = []() {
    ToolTabFactory::instance().registerTab("1", [](FileDataBuffer* buffer) {
        return new CodeEditorTab(buffer);
    });
    return true;
}();

CodeEditorTab::CodeEditorTab(FileDataBuffer* buffer, QWidget* parent)
    : ToolTab(buffer, parent)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_searchBar = new QWidget(this);
    auto* searchLayout = new QHBoxLayout(m_searchBar);
    searchLayout->setContentsMargins(8, 6, 8, 6);
    searchLayout->setSpacing(6);
    m_searchBar->setStyleSheet("background-color: #252525; border-bottom: 1px solid #3a3a3a;");

    auto* searchLabel = new QLabel("Find:", m_searchBar);
    m_searchEdit = new QLineEdit(m_searchBar);
    m_searchEdit->setPlaceholderText("Search in file");
    m_searchPrevButton = new QPushButton("Prev", m_searchBar);
    m_searchNextButton = new QPushButton("Next", m_searchBar);
    m_matchCaseCheckBox = new QCheckBox("Match case", m_searchBar);
    m_searchStatusLabel = new QLabel("0/0", m_searchBar);
    m_searchCloseButton = new QPushButton("x", m_searchBar);
    m_searchCloseButton->setFixedWidth(28);

    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchEdit, 1);
    searchLayout->addWidget(m_searchPrevButton);
    searchLayout->addWidget(m_searchNextButton);
    searchLayout->addWidget(m_matchCaseCheckBox);
    searchLayout->addWidget(m_searchStatusLabel);
    searchLayout->addWidget(m_searchCloseButton);
    m_searchBar->hide();
    rootLayout->addWidget(m_searchBar);

    m_codeEditorWidget = new CustomCodeEditor(this);

    m_overlayWidget = new QWidget(this);
    auto overlayLayout = new QVBoxLayout(m_overlayWidget);
    overlayLayout->setAlignment(Qt::AlignCenter);

    QLabel* title = new QLabel("Binary file detected", m_overlayWidget);
    title->setStyleSheet("color: white; font-size: 20px;");
    title->setAlignment(Qt::AlignCenter);
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    overlayLayout->addWidget(title);
    overlayLayout->addSpacing(15);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->setAlignment(Qt::AlignCenter);
    auto* anywayOpenBtn = new QPushButton("Open anyway", m_overlayWidget);
    btnLayout->addWidget(anywayOpenBtn);
    overlayLayout->addLayout(btnLayout);

    auto* stackHost = new QWidget(this);
    auto* stack = new QStackedLayout(stackHost);
    stack->setStackingMode(QStackedLayout::StackAll);
    stack->addWidget(m_codeEditorWidget);
    stack->addWidget(m_overlayWidget);
    rootLayout->addWidget(stackHost);

    m_overlayWidget->hide();

    connect(anywayOpenBtn, &QPushButton::clicked, this, [this]() {
        forceSetData = true;
        setTabData();
    });

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_lastSearchText = text;
        updateSearchUi();
    });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() { findNext(true); });
    connect(m_searchPrevButton, &QPushButton::clicked, this, [this]() { findNext(false); });
    connect(m_searchNextButton, &QPushButton::clicked, this, [this]() { findNext(true); });
    connect(m_searchCloseButton, &QPushButton::clicked, this, &CodeEditorTab::closeSearchBar);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(m_matchCaseCheckBox, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState) {
        updateSearchUi();
    });
#else
    connect(m_matchCaseCheckBox, &QCheckBox::stateChanged, this, [this](int) {
        updateSearchUi();
    });
#endif

    connect(m_codeEditorWidget, &CustomCodeEditor::contentsChanged, this, [this]() {
        if (m_dataBuffer->isModified()) {
            setModifyIndicator(true);
            emit modifyData();
        } else {
            setModifyIndicator(false);
            emit dataEqual();
        }
    });

    connect(m_codeEditorWidget, &CustomCodeEditor::modificationChanged, this, [this](bool modified) {
        setModifyIndicator(modified);
        if (modified)
            emit modifyData();
        else
            emit dataEqual();
    });

    m_findShortcut = new QShortcut(QKeySequence::Find, this);
    m_findNextShortcut = new QShortcut(QKeySequence(Qt::Key_F3), this);
    m_findPreviousShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3), this);
    m_goToLineShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_G), this);

    connect(m_findShortcut, &QShortcut::activated, this, &CodeEditorTab::openFindDialog);
    connect(m_findNextShortcut, &QShortcut::activated, this, [this]() { findNext(true); });
    connect(m_findPreviousShortcut, &QShortcut::activated, this, [this]() { findNext(false); });
    connect(m_goToLineShortcut, &QShortcut::activated, this, &CodeEditorTab::openGoToLineDialog);
}

void CodeEditorTab::openFindDialog()
{
    m_searchBar->show();
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
    updateSearchUi();
}

void CodeEditorTab::findNext(bool forward)
{
    if (m_lastSearchText.isEmpty()) {
        openFindDialog();
        return;
    }

    const Qt::CaseSensitivity caseSensitivity = m_matchCaseCheckBox->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    m_codeEditorWidget->findText(m_lastSearchText, forward, caseSensitivity);
    updateSearchUi();
}

void CodeEditorTab::openGoToLineDialog()
{
    bool ok = false;
    const int line = QInputDialog::getInt(this,
                                          tr("Go to line"),
                                          tr("Line number:"),
                                          1,
                                          1,
                                          static_cast<int>(qMax<qint64>(1, m_codeEditorWidget->lineCount())),
                                          1,
                                          &ok);
    if (!ok)
        return;

    m_codeEditorWidget->goToLine(line);
}

void CodeEditorTab::updateSearchUi()
{
    const Qt::CaseSensitivity caseSensitivity = m_matchCaseCheckBox->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const int total = m_codeEditorWidget->countMatches(m_searchEdit->text(), caseSensitivity);
    const int current = m_codeEditorWidget->currentMatchIndex(m_searchEdit->text(), caseSensitivity);
    m_searchStatusLabel->setText(QString("%1/%2").arg(current).arg(total));

    const bool hasQuery = !m_searchEdit->text().isEmpty();
    m_searchPrevButton->setEnabled(hasQuery && total > 0);
    m_searchNextButton->setEnabled(hasQuery && total > 0);
}

void CodeEditorTab::closeSearchBar()
{
    m_searchBar->hide();
    m_codeEditorWidget->setFocus();
}

void CodeEditorTab::setFile(QString filepath)
{
    m_fileContext = new FileContext(filepath);
    m_codeEditorWidget->setFileExt(CustomCodeEditor::syntaxKeyForPath(filepath));
}

void CodeEditorTab::setTabData()
{
    static constexpr qint64 kLargeTextFileThreshold = 2 * 1024 * 1024;

    qDebug() << "CodeEditorTab::setTabData this=" << this << " buffer=" << m_dataBuffer;
    const QByteArray probeData = m_dataBuffer->read(0, 4096);
    const bool binary = isBinary(probeData);
    qDebug() << "CodeEditorTab::setTabData binary=" << binary << " forceSetData=" << forceSetData << " probeSize=" << probeData.size();

    if (binary && !forceSetData) {
        m_codeEditorWidget->hide();
        m_overlayWidget->show();
    } else {
        m_overlayWidget->hide();
        m_codeEditorWidget->show();
        const bool largeFileMode = m_dataBuffer->isLargeFile() || m_dataBuffer->size() >= kLargeTextFileThreshold;
        if (m_largeFileMode != largeFileMode) {
            m_largeFileMode = largeFileMode;
            if (m_largeFileMode) {
                m_codeEditorWidget->setWordWrapEnabled(false);
                m_codeEditorWidget->setSyntaxHighlighter(nullptr);
            } else {
                m_codeEditorWidget->setWordWrapEnabled(true);
                m_codeEditorWidget->setFileExt(CustomCodeEditor::syntaxKeyForPath(m_fileContext->filePath()));
            }
        }
        m_codeEditorWidget->setBuffer(m_dataBuffer);
        forceSetData = false;
    }

    if (m_dataBuffer->isModified()) {
        setModifyIndicator(true);
        emit modifyData();
    } else {
        setModifyIndicator(false);
        qDebug() << "CodeEditorTab::setTabData emit dataEqual this=" << this;
        emit dataEqual();
        qDebug() << "CodeEditorTab::setTabData dataEqual returned this=" << this;
    }
}

void CodeEditorTab::onDataChanged()
{
    if (m_dataBuffer->isModified()) {
        setModifyIndicator(true);
        emit modifyData();
    } else {
        setModifyIndicator(false);
        emit dataEqual();
    }
}

void CodeEditorTab::onSelectionChanged(qint64 pos, qint64 length)
{
    Q_UNUSED(pos);
    Q_UNUSED(length);
    if (m_updatingSelection)
        return;
}

void CodeEditorTab::saveTabData()
{
    if (!m_dataBuffer->isModified())
        return;

    if (!m_dataBuffer->saveToFile(m_fileContext->filePath()))
        return;

    setModifyIndicator(false);
    emit dataEqual();
    emit refreshDataAllTabsSignal();
}

void CodeEditorTab::goToLine(int lineNumber, bool selectWholeLine)
{
    if (lineNumber < 1)
        return;
    if (m_overlayWidget && !m_overlayWidget->isHidden())
        return;
    m_codeEditorWidget->goToLine(lineNumber, selectWholeLine);
}

void CodeEditorTab::goToSearchHit(int lineNumber, const QString &needle)
{
    if (lineNumber < 1)
        return;
    if (m_overlayWidget && !m_overlayWidget->isHidden())
        return;
    m_codeEditorWidget->revealSearchHit(lineNumber, needle);
}
