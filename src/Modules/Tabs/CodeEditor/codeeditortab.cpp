#include "codeeditortab.h"
#include "utils/utils.h"
#include "libs/CodeEditor/include/widgets/CustomCodeEditor.h"
#include "libs/CodeEditor/include/languages/LanguageRegistry.h"
#include "core/modules/ModuleManager.h"
#include "core/settings/appsettings.h"
#include "core/git/gitmanager.h"

#include <QBoxLayout>
#include <QFileInfo>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QStackedLayout>
#include <QVBoxLayout>

static QString displayName() {
    return QCoreApplication::translate("CodeEditorTab", "Code");
}

static bool registered = []() {
    ModuleManager::instance().registerModule<TabBase>(&displayName, "always", []() { return new CodeEditorTab(); }, 100);
    return true;
}();

CodeEditorTab::CodeEditorTab(QWidget* parent)
    : TabBase(parent)
{

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // - - Code Editor - -
    m_codeEditorWidget = new CustomCodeEditor(this);
    m_overlayWidget = new QWidget(this);
    auto overlayLayout = new QVBoxLayout(m_overlayWidget);
    overlayLayout->setAlignment(Qt::AlignCenter);

    QLabel* title = new QLabel(tr("Binary file detected"), m_overlayWidget);
    title->setStyleSheet("color: white; font-size: 20px;");
    title->setAlignment(Qt::AlignCenter);
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    overlayLayout->addWidget(title);
    overlayLayout->addSpacing(15);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->setAlignment(Qt::AlignCenter);
    auto* anywayOpenBtn = new QPushButton(tr("Open anyway"), m_overlayWidget);
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

    connect(m_codeEditorWidget, &CustomCodeEditor::cursorPositionChanged, this, [this]() {
        emit statusBarInfoChanged(
            QString("Ln %1, Col %2    %3")
                .arg(m_codeEditorWidget->cursorLine())
                .arg(m_codeEditorWidget->cursorColumn())
                .arg(m_currentLang));
    });

    connect(m_codeEditorWidget, &CustomCodeEditor::modificationChanged, this, [this](bool modified) {
        setModifyIndicator(modified);
        if (modified)
            emit modifyData();
        else
            emit dataEqual();
    });

    connect(m_codeEditorWidget, &CustomCodeEditor::contentsChanged, this, [this]() {
        if (!m_dataBuffer)
            return;

        if (m_dataBuffer->isModified()) {
            setModifyIndicator(true);
            emit modifyData();
        } else {
            setModifyIndicator(false);
            emit dataEqual();
        }
    });

    m_goToLineShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_G), this);

    connect(m_goToLineShortcut, &QShortcut::activated, this, &CodeEditorTab::openGoToLineDialog);

    /* Blame Worker Setup */
    m_blameThread = new QThread(this);
    m_blameWorker = new GitBlameWorker();
    m_blameWorker->moveToThread(m_blameThread);

    connect(m_blameThread, &QThread::finished, m_blameWorker, &QObject::deleteLater);
    connect(this, &CodeEditorTab::destroyed, m_blameThread, &QThread::quit);

    connect(m_blameWorker, &GitBlameWorker::blameFinished, this, &CodeEditorTab::onBlameFinished);

    m_blameThread->start();

    m_codeEditorWidget->setGitBlameEnabled(AppSettings::gitBlameEnabled());
    m_codeEditorWidget->setGitBlameColor(AppSettings::gitBlameColor());
    m_codeEditorWidget->setGitBlamePadding(AppSettings::gitBlamePadding());

    connect(SettingsNotifier::instance(), &SettingsNotifier::gitBlameEnabledChanged,
            this, &CodeEditorTab::setGitBlameSlot);
    connect(SettingsNotifier::instance(), &SettingsNotifier::gitBlameColorChanged,
            m_codeEditorWidget, &CustomCodeEditor::setGitBlameColor);
    connect(SettingsNotifier::instance(), &SettingsNotifier::gitBlamePaddingChanged,
            m_codeEditorWidget, &CustomCodeEditor::setGitBlamePadding);

    connect(GitNotifier::instance(), &GitNotifier::repositoryChanged,
            this, &CodeEditorTab::requestBlameUpdate);
}

void CodeEditorTab::setFileDataBuffer(FileDataBuffer* newFileDataBuffer) {
    if (m_dataBuffer == newFileDataBuffer)
        return;

    TabBase::setFileDataBuffer(newFileDataBuffer);
    m_codeEditorWidget->setBuffer(newFileDataBuffer);
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

QString CodeEditorTab::selectedSearchText() const
{
    const QString text = m_codeEditorWidget->selectedText();
    return text.contains(QLatin1Char('\n')) || text.contains(QLatin1Char('\r')) ? QString() : text;
}

bool CodeEditorTab::revealSearchMatch(int oneBasedLine, int zeroBasedColumn, int length)
{
    return m_codeEditorWidget->selectTextRange(oneBasedLine, zeroBasedColumn, length);
}

void CodeEditorTab::setFile(QString filepath)
{
    m_fileContext = new FileContext(filepath);
    m_codeEditorWidget->setFileExt(CustomCodeEditor::syntaxKeyForPath(filepath));
    m_currentLang = detectLanguage(filepath);

    m_repoRoot = GitManager::findGitRepositoryRoot(QFileInfo(filepath).absolutePath());
    requestBlameUpdate();
}

void CodeEditorTab::requestBlameUpdate()
{
    if (m_repoRoot.isEmpty() || !m_codeEditorWidget->isGitBlameEnabled() || m_largeFileMode) {
        m_codeEditorWidget->setBlameData({});
        return;
    }

    QMetaObject::invokeMethod(m_blameWorker, "runBlame",
                              Qt::QueuedConnection,
                              Q_ARG(QString, m_repoRoot),
                              Q_ARG(QString, m_fileContext->filePath()));
}

void CodeEditorTab::onBlameFinished(const QVector<BlameLineInfo> &result)
{
    m_codeEditorWidget->setBlameData(result);
}

void CodeEditorTab::setGitBlameSlot(bool checked)
{
    if (m_codeEditorWidget->isGitBlameEnabled() == checked)
        return;

    m_codeEditorWidget->setGitBlameEnabled(checked);
    if (checked) {
        requestBlameUpdate();
    } else {
        m_codeEditorWidget->setBlameData({});
    }
}

QString CodeEditorTab::detectLanguage(const QString& filePath)
{
    // Single source of truth: LanguageRegistry. See docs/adding_a_language.md.
    const QFileInfo fi(filePath);
    const LanguageDefinition& language = LanguageRegistry::instance().resolveForFile(fi.fileName());
    if (&language != &LanguageRegistry::plainTextLanguage())
        return language.displayName;

    // Unknown extension: fall back to the extension itself (uppercased) so the
    // status bar still shows something useful, matching the previous behavior.
    const QString ext = fi.suffix();
    return ext.isEmpty() ? QStringLiteral("Plain Text") : ext.toUpper();
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
                m_codeEditorWidget->setGitBlameEnabled(false);
            } else {
                m_codeEditorWidget->setWordWrapEnabled(true);
                m_codeEditorWidget->setFileExt(CustomCodeEditor::syntaxKeyForPath(m_fileContext->filePath()));
                m_codeEditorWidget->setGitBlameEnabled(AppSettings::gitBlameEnabled());
            }
        }
        m_codeEditorWidget->setBuffer(m_dataBuffer);
        requestBlameUpdate();
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

    requestBlameUpdate();
    setModifyIndicator(false);
    emit dataEqual();
    emit refreshDataAllTabsSignal();
}

void CodeEditorTab::setWordWrapSlot(bool checked) {
    m_codeEditorWidget->setWordWrapEnabled(checked);
}

void CodeEditorTab::setTabReplaceSlot(bool checked) {
    m_codeEditorWidget->setTabReplace(checked);
}

void CodeEditorTab::setTabWidthSlot(int width) {
    m_codeEditorWidget->setTabDisplaySize(width);
    m_codeEditorWidget->setTabReplaceSize(width);
}
