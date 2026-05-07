#include "disassemblertab.h"
#include "disassemblerworker.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QClipboard>
#include <QListWidget>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QThread>
#include <QFont>
#include <QColor>
#include <QDateTime>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTimer>
#include <QToolTip>
#include <QHelpEvent>

#include <QGuiApplication>
#include <QClipboard>

#include "utils/appsettings.h"
#include "utils/instructionhelpservice.h"
#include "disasm/disasmtexthighlighter.h"
#include "core/ToolTabFactory.h"

static bool registered = [](){
    ToolTabFactory::instance().registerTab("3", [](FileDataBuffer* buffer){
        return new DisassemblerTab(buffer);
    });
    return true;
}();

static QString normalizeBytes(const QString &bytes)
{
    QString s = bytes;
    s.remove(' ');
    s.remove('\t');
    s.remove('\n');
    s.remove('\r');
    return s.trimmed();
}

static bool parseHexU64(const QString &s, quint64 *out)
{
    if (!out) return false;
    QString t = s.trimmed();
    if (t.startsWith("0x", Qt::CaseInsensitive)) t = t.mid(2);
    bool ok = false;
    const quint64 v = t.toULongLong(&ok, 16);
    if (!ok) return false;
    *out = v;
    return true;
}

static QByteArray bytesToArray(const QString &hex, bool *okOut = nullptr)
{
    QString s = normalizeBytes(hex);
    if (s.startsWith("0x", Qt::CaseInsensitive)) s = s.mid(2);
    if (s.size() % 2 != 0) { if (okOut) *okOut = false; return {}; }
    QByteArray out;
    out.reserve(s.size() / 2);
    for (int i = 0; i < s.size(); i += 2) {
        bool ok = false;
        const int b = s.mid(i, 2).toInt(&ok, 16);
        if (!ok) { if (okOut) *okOut = false; return {}; }
        out.append(static_cast<char>(b & 0xFF));
    }
    if (okOut) *okOut = true;
    return out;
}

bool DisassemblerTab::tryResolveStringRefAddr(const LineInfo &li, quint64 *outAddr) const
{
    if (!outAddr) return false;
    *outAddr = 0;
    if (m_stringByAddr.isEmpty()) return false;

    /* Direct absolute address in operands. */
    {
        static const QRegularExpression reHex(R"((0x[0-9a-fA-F]+))");
        auto it = reHex.globalMatch(li.operands);
        while (it.hasNext()) {
            const auto m = it.next();
            quint64 addr = 0;
            if (!parseHexU64(m.captured(1), &addr))
                continue;
            if (m_stringByAddr.contains(addr)) {
                *outAddr = addr;
                return true;
            }
        }
    }

    /* RIP-relative (AT&T): disp(%rip) **
    {
        static const QRegularExpression reRip(R"(([+-]?(?:0x[0-9a-fA-F]+|\d+))\s*\(%rip\))",
                                            QRegularExpression::CaseInsensitiveOption);
        const auto m = reRip.match(li.operands);
        if (!m.hasMatch()) return false;

        quint64 insn = 0;
        if (!parseHexU64(li.address, &insn)) return false;
        const QString b = normalizeBytes(li.bytes);
        const quint64 len = static_cast<quint64>(b.size() / 2);

        const QString dispStr = m.captured(1).trimmed();
        bool okDisp = false;
        qlonglong disp = 0;
        QString d = dispStr;
        if (d.startsWith("+")) d = d.mid(1);
        if (d.startsWith("-")) {
            QString mag = d.mid(1);
            qulonglong u = 0;
            if (mag.startsWith("0x", Qt::CaseInsensitive))
                u = mag.mid(2).toULongLong(&okDisp, 16);
            else
                u = mag.toULongLong(&okDisp, 10);
            if (okDisp) disp = -static_cast<qlonglong>(u);
        } else {
            qulonglong u = 0;
            if (d.startsWith("0x", Qt::CaseInsensitive))
                u = d.mid(2).toULongLong(&okDisp, 16);
            else
                u = d.toULongLong(&okDisp, 10);
            if (okDisp) disp = static_cast<qlonglong>(u);
        }
        if (!okDisp) return false;

        const quint64 next = insn + len;
        const quint64 target = static_cast<quint64>(static_cast<qlonglong>(next) + disp);
        if (!m_stringByAddr.contains(target)) return false;
        *outAddr = target;
        return true;
    }
}

QString DisassemblerTab::autoCommentForLine(const LineInfo &li) const
{
    /* Auto-comment string references like IDA: ; "Hello world" */
    if (m_stringByAddr.isEmpty())
        return {};
    if (li.operands.isEmpty())
        return {};

    auto render = [](QString v) {
        v.replace('\n', "\\n");
        v.replace('\r', "\\r");
        if (v.size() > 120) v = v.left(120) + "…";
        return QString("; \"%1\"").arg(v);
    };

    quint64 addr = 0;
    if (tryResolveStringRefAddr(li, &addr)) {
        const QString v = m_stringByAddr.value(addr);
        const int len = v.toUtf8().size(); // byte length without trailing 0
        return render(v) + QString(" (%1)").arg(len);
    }

    return {};
}

QString DisassemblerTab::formatLine(const LineInfo &li) const
{
    /* 1. Adress (18 symbols for 64-byte cores) */
    QString addr = li.address.trimmed();
    if (!addr.startsWith("0x")) addr = "0x" + addr;
    addr = addr.leftJustified(18, ' ');

    /* 2. Label funciton (e.g. <sys_get_time_ms>) */
    QString mnem = li.mnemonic.trimmed();
    if (mnem.startsWith('<') && mnem.endsWith('>')) {
        return QString("%1 %2").arg(addr, mnem);
    }

    /* 3. Bytes (preview up to 8 bytes, hide the rest under '..') */
    QString bytes;
    const QString b = normalizeBytes(li.bytes);
    const int totalBytes = b.size() / 2;
    const int previewBytes = qMin(8, totalBytes);
    if (totalBytes > 0) {
        QString spaced;
        for (int i = 0; i < previewBytes * 2; i += 2) {
            if (i) spaced += ' ';
            spaced += b.mid(i, 2);
        }
        if (totalBytes > previewBytes) spaced += " …";
        bytes = spaced;
    }
    bytes = bytes.leftJustified(24, ' '); // Fixed byte width

    /* 4. Mnemonic and operand alignment */
    mnem = mnem.leftJustified(10, ' '); // 10 characters for mnemonic
    
    QString ops = li.operands.trimmed();
    if (ops.contains('#')) ops = ops.section('#', 0, 0).trimmed(); // clean objdump components
    ops = ops.leftJustified(32, ' '); // 32 символа на операнды

    /* 5. Comments (Auto-Comments for string) */
    const QString comment = autoCommentForLine(li);
    const QString c = comment.isEmpty() ? QString() : ("  " + comment);

    /* Handle "invalid" if raw data encountered */
    if (li.mnemonic.trimmed().compare("invalid", Qt::CaseInsensitive) == 0 && !li.bytes.trimmed().isEmpty()) {
        QStringList parts;
        for (int i = 0; i + 1 < b.size() && (i / 2) < previewBytes; i += 2)
            parts << ("0x" + b.mid(i, 2));

        QString data = QString(".byte %1").arg(parts.join(", "));
        if (totalBytes > previewBytes) data += ", …";
        
        data = data.leftJustified(32, ' ');
        const QString invInfo = QString("  ; invalid bytes (%1)").arg(totalBytes);
        return QString("%1  %2  %3 %4%5").arg(addr, bytes, QString("invalid").leftJustified(10, ' '), data, invInfo);
    }

    /* Final assembly with hard indentation */
    return QString("%1  %2  %3 %4%5").arg(addr, bytes, mnem, ops, c);
}

// ─────────────────────────────────────────────────────────────────────────────
DisassemblerTab::DisassemblerTab(FileDataBuffer* buffer, QWidget *parent)
    : ToolTab{buffer, parent}
{
    setupUi();

    m_thread = new QThread(this);
    m_worker = new DisassemblerWorker();
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::finished,       m_worker, &QObject::deleteLater);
    connect(this,  &DisassemblerTab::requestDisassembly,
            m_worker, &DisassemblerWorker::disassemble, Qt::QueuedConnection);
    connect(m_worker, &DisassemblerWorker::sectionFound,
            this,     &DisassemblerTab::onSectionFound,    Qt::QueuedConnection);
    connect(m_worker, &DisassemblerWorker::functionsFound,
            this,     &DisassemblerTab::onFunctionsFound,  Qt::QueuedConnection);
    connect(m_worker, &DisassemblerWorker::stringsFound,
            this,     &DisassemblerTab::onStringsFound,    Qt::QueuedConnection);
    connect(m_worker, &DisassemblerWorker::finished,
            this,     &DisassemblerTab::onWorkerFinished,  Qt::QueuedConnection);
    connect(m_worker, &DisassemblerWorker::errorOccurred,
            this,     &DisassemblerTab::onWorkerError,     Qt::QueuedConnection);
    connect(m_worker, &DisassemblerWorker::progressUpdated,
            this,     &DisassemblerTab::onProgressUpdated, Qt::QueuedConnection);
    connect(m_worker, &DisassemblerWorker::logLine,
            this,     &DisassemblerTab::onLogLine,         Qt::QueuedConnection);

    m_thread->start();

    /*connect(&GlobalWidgetsManager::instance(), &GlobalWidgetsManager::actionTriggered,
            this, &DisassemblerTab::onGlobalActionTriggered);*/

    updateBackendUiHint();
}

DisassemblerTab::~DisassemblerTab()
{
    if (m_worker) m_worker->cancel();
    m_thread->quit();
    m_thread->wait(2000);
}

void DisassemblerTab::setFile(QString filepath){
    m_fileContext = new FileContext(filepath);
}

void DisassemblerTab::setTabData()
{
    startDisassembly();
}

void DisassemblerTab::saveTabData()
{
    if (!m_dataBuffer->isModified())
        return;

    if (!m_dataBuffer->saveToFile(m_fileContext->filePath()))
        return;

    setModifyIndicator(false);
    emit dataEqual();
}

void DisassemblerTab::onSelectionChanged(qint64 pos, qint64 length)
{
    if (m_updatingSelection) return;

    m_updatingSelection = true;

    if (m_lines.isEmpty()) {
        m_updatingSelection = false;
        return;
    }

    int bestVisibleLine = -1;
    const qint64 selEnd = pos + qMax<qint64>(length, 1);
    for (int visLine = 0; visLine < m_visibleLineMap.size(); ++visLine) {
        const int idx = m_visibleLineMap[visLine];
        if (idx < 0 || idx >= m_lines.size())
            continue;

        const LineInfo &li = m_lines[idx];
        if (li.fileOffset < 0 || li.size <= 0)
            continue;

        const qint64 lineStart = li.fileOffset;
        const qint64 lineEnd = li.fileOffset + li.size;
        if (pos < lineEnd && selEnd > lineStart) {
            bestVisibleLine = visLine;
            break;
        }
    }

    if (bestVisibleLine >= 0) {
        QTextCursor cursor = m_disasmView->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, bestVisibleLine);
        cursor.select(QTextCursor::LineUnderCursor);
        m_disasmView->setTextCursor(cursor);
        m_disasmView->ensureCursorVisible();
    }

    m_updatingSelection = false;
}

void DisassemblerTab::onDataChanged()
{
    if (!m_refreshDebounce) {
        m_refreshDebounce = new QTimer(this);
        m_refreshDebounce->setSingleShot(true);
        connect(m_refreshDebounce, &QTimer::timeout, this, [this]() {
            if (!m_running)
                startDisassembly();
        });
    }

    if (!m_running)
        m_refreshDebounce->start(150);
}

/*─────────────────────────────────────────────────────────────────────────────*/
void DisassemblerTab::setupUi()
{
    
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    /*── Toolbar ──────────────────────────────────────────────────────────────*/
    m_toolbar = new QWidget(this);
    m_toolbar->setObjectName("dasmToolbar");
    m_toolbar->setFixedHeight(36);
    auto *toolLayout = new QHBoxLayout(m_toolbar);
    toolLayout->setContentsMargins(6, 0, 6, 0);
    toolLayout->setSpacing(6);

    m_runBtn = new QPushButton(tr("Disassemble"), m_toolbar);
    m_runBtn->setFixedHeight(26);
    toolLayout->addWidget(m_runBtn);

    m_cancelBtn = new QPushButton(tr("Cancel"), m_toolbar);
    m_cancelBtn->setFixedHeight(26);
    m_cancelBtn->setVisible(false);
    toolLayout->addWidget(m_cancelBtn);

    toolLayout->addSpacing(8);

    QLabel *secLabel = new QLabel(tr("Section:"), m_toolbar);
    toolLayout->addWidget(secLabel);

    m_sectionCombo = new QComboBox(m_toolbar);
    m_sectionCombo->setFixedHeight(26);
    m_sectionCombo->setMinimumWidth(140);
    m_sectionCombo->setEnabled(false);
    toolLayout->addWidget(m_sectionCombo);

    toolLayout->addSpacing(8);

    m_searchEdit = new QLineEdit(m_toolbar);
    m_searchEdit->setPlaceholderText(tr("Search address / mnemonic / operands…"));
    m_searchEdit->setFixedHeight(26);
    m_searchEdit->setEnabled(false);
    toolLayout->addWidget(m_searchEdit, 1);

    toolLayout->addSpacing(8);

    m_logToggleBtn = new QPushButton(tr("Log"), m_toolbar);
    m_logToggleBtn->setFixedHeight(26);
    m_logToggleBtn->setFixedWidth(50);
    m_logToggleBtn->setCheckable(true);
    m_logToggleBtn->setChecked(false);
    m_logToggleBtn->setToolTip(tr("Toggle diagnostic log panel"));
    toolLayout->addWidget(m_logToggleBtn);

    mainLayout->addWidget(m_toolbar);

    /*── Progress bar ──────────────────────────────────────────────────────────*/
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(3);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    /*── Splitter: disasm table (top) + log panel (bottom) ────────────────────*/
    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->setHandleWidth(4);
    mainLayout->addWidget(m_splitter, 1);

    /*── Top: stack (listing / placeholder) ───────────────────────────────────*/
    QWidget *topWidget = new QWidget(m_splitter);
    auto *topLayout = new QVBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);

    m_stack = new QStackedWidget(topWidget);
    topLayout->addWidget(m_stack);

    m_placeholderLbl = new QLabel(topWidget);
    m_placeholderLbl->setAlignment(Qt::AlignCenter);
    m_stack->addWidget(m_placeholderLbl);

    /* Functions panel + listing */
    m_topSplitter = new QSplitter(Qt::Horizontal, topWidget);
    m_topSplitter->setHandleWidth(4);

    m_funcPanel = new QWidget(m_topSplitter);
    auto *funcLayout = new QVBoxLayout(m_funcPanel);
    funcLayout->setContentsMargins(6, 6, 6, 6);
    funcLayout->setSpacing(6);
    auto *funcTitle = new QLabel(tr("Functions"), m_funcPanel);
    funcTitle->setStyleSheet("color:#60a5fa; font-weight:600;");
    funcLayout->addWidget(funcTitle);
    m_funcFilterEdit = new QLineEdit(m_funcPanel);
    m_funcFilterEdit->setPlaceholderText(tr("Filter…"));
    funcLayout->addWidget(m_funcFilterEdit);
    m_funcList = new QListWidget(m_funcPanel);
    m_funcList->setObjectName("disasmFuncList");
    funcLayout->addWidget(m_funcList, 1);
    m_funcPanel->setLayout(funcLayout);

    connect(m_funcFilterEdit, &QLineEdit::textChanged, this, [this](const QString &q) {
        const QString qq = q.trimmed().toLower();
        for (int i = 0; i < m_funcList->count(); ++i) {
            auto *it = m_funcList->item(i);
            const QString t = it->text().toLower();
            it->setHidden(!qq.isEmpty() && !t.contains(qq));
        }
    });
    connect(m_funcList, &QListWidget::itemActivated, this, [this](QListWidgetItem *it) {
        if (it) jumpToAddress(it->data(Qt::UserRole).toString());
    });

    connect(m_funcList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        if (it) jumpToAddress(it->data(Qt::UserRole).toString());
    });

    m_disasmView = new QPlainTextEdit(m_topSplitter);
    m_disasmView->setReadOnly(true);
    m_disasmView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_disasmView->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_disasmView->setStyleSheet(
        "QPlainTextEdit { "
        "font-family: 'JetBrains Mono', 'Consolas', 'DejaVu Sans Mono', 'Ubuntu Mono', 'Liberation Mono', monospace; "
        "font-size: 12px; "
        "}"
    );
    m_disasmView->setContextMenuPolicy(Qt::CustomContextMenu);
    QFont f = m_disasmView->font();
    f.setStyleHint(QFont::Monospace);
    f.setFixedPitch(true);
    m_disasmView->setFont(f);
    m_disasmView->viewport()->setMouseTracking(true);
    m_disasmView->viewport()->installEventFilter(this);
    m_disasmHighlighter = new DisasmTextHighlighter(m_disasmView->document());
    connect(m_disasmView, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        if (!m_disasmView || !m_disasmView->hasFocus())
            return;
        const QPoint p = m_disasmView->cursorRect().bottomRight();
        showInstructionHelpAt(p, true);
    });

    /* Disassembler selection handler — notify buffer */
    connect(m_disasmView, &QPlainTextEdit::selectionChanged, this, [this]() {
        if (m_updatingSelection) return; // Recursion prevention
        
        QTextCursor cursor = m_disasmView->textCursor();
        if (!cursor.hasSelection()) return;
        
        int visLine = cursor.blockNumber();
        if (visLine < 0 || visLine >= m_visibleLineMap.size()) return;
        
        int idx = m_visibleLineMap[visLine];
        if (idx < 0 || idx >= m_lines.size()) return;
        
        const LineInfo& li = m_lines[idx];
        if (li.address.isEmpty()) return;
        
        if (li.fileOffset >= 0 && li.size > 0)
            m_dataBuffer->setSelection(li.fileOffset, li.size);
    });

    connect(m_disasmView, &QPlainTextEdit::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (!m_disasmView) return;
        const QTextCursor c = m_disasmView->cursorForPosition(pos);
        const int visLine = c.blockNumber();
        if (visLine < 0 || visLine >= m_visibleLineMap.size()) return;
        const int idx = m_visibleLineMap[visLine];
        if (idx < 0 || idx >= m_lines.size()) return; // ignore header/separator lines
        const LineInfo &li = m_lines[idx];

        QMenu menu(this);
        menu.addAction(tr("Copy address"), [li]() {
            QGuiApplication::clipboard()->setText(li.address.trimmed());
        });
        if (!li.bytes.trimmed().isEmpty()) {
            menu.addAction(tr("Copy bytes"), [li]() {
                QGuiApplication::clipboard()->setText(li.bytes.trimmed());
            });
        }

        /* Safe patching: only for radare2 backend where address is an offset we can write to. **
        const bool canPatchAtLine = (AppSettings::disasmBackend() == AppSettings::DisasmBackend::Radare2);
        if (canPatchAtLine) {
            menu.addSeparator();
            menu.addAction(tr("Hex patch here…"), [this, li]() {
                if (li.fileOffset < 0) {
                    QMessageBox::warning(this, tr("Patch"), tr("No file offset mapping for this instruction."));
                    return;
                }

                bool ok = false;
                const QString text = QInputDialog::getText(
                    this,
                    tr("Hex patch"),
                    tr("Hex bytes to write at %1:").arg(li.address.trimmed()),
                    QLineEdit::Normal,
                    QString(),
                    &ok);
                if (!ok) return;

                bool okBytes = false;
                const QByteArray newBytes = bytesToArray(text, &okBytes);
                if (!okBytes || newBytes.isEmpty()) {
                    QMessageBox::warning(this, tr("Patch"), tr("Invalid hex bytes."));
                    return;
                }

                m_dataBuffer->setBytes(li.fileOffset, newBytes);
                setModifyIndicator(m_dataBuffer->isModified());
                emit modifyData();

                appendLog(QString("[hexpatch] wrote %1 bytes at 0x%2")
                              .arg(newBytes.size())
                              .arg(QString::number(li.fileOffset, 16)));
                if (m_running) cancelDisassembly();
                startDisassembly();
            });

            menu.addAction(tr("Patch bytes…"), [this, idx, li]() {
                if (li.fileOffset < 0) {
                    QMessageBox::warning(this, tr("Patch"), tr("No file offset mapping for this instruction."));
                    return;
                }

                bool ok = false;
                const QString cur = li.bytes.trimmed();
                const QString text = QInputDialog::getText(
                    this,
                    tr("Patch bytes"),
                    tr("Hex bytes (same length):"),
                    QLineEdit::Normal,
                    cur,
                    &ok);
                if (!ok) return;

                bool okBytes = false;
                const QByteArray newBytes = bytesToArray(text, &okBytes);
                if (!okBytes) {
                    QMessageBox::warning(this, tr("Patch"), tr("Invalid hex bytes."));
                    return;
                }

                bool okOld = false;
                const QByteArray oldBytes = bytesToArray(cur, &okOld);
                if (!okOld) {
                    QMessageBox::warning(this, tr("Patch"), tr("Current bytes are invalid."));
                    return;
                }
                if (newBytes.size() != oldBytes.size()) {
                    QMessageBox::warning(this, tr("Patch"), tr("Byte length must match (%1 bytes).").arg(oldBytes.size()));
                    return;
                }

                m_dataBuffer->setBytes(li.fileOffset, newBytes);
                setModifyIndicator(m_dataBuffer->isModified());
                emit modifyData();

                /* Update cached bytes and refresh view. */
                m_lines[idx].bytes = normalizeBytes(text);
                m_lines[idx].bytesL = m_lines[idx].bytes.toLower();
                m_lines[idx].size = newBytes.size();
                applyFilter();
                appendLog(QString("[patch] wrote %1 bytes at 0x%2")
                              .arg(newBytes.size())
                              .arg(QString::number(li.fileOffset, 16)));
            });
        }

        /* Show raw bytes for referenced string (helps confirm full length incl. 00). */
        quint64 saddr = 0;
        if (tryResolveStringRefAddr(li, &saddr)) {
            menu.addSeparator();
            if (canPatchAtLine) {
                menu.addAction(tr("Hex patch string…"), [this, saddr]() {
                    bool ok = false;
                    const QString text = QInputDialog::getText(
                        this,
                        tr("Hex patch string"),
                        tr("Hex bytes to write at string addr 0x%1:").arg(QString::number(saddr, 16)),
                        QLineEdit::Normal,
                        QString(),
                        &ok);
                    if (!ok) return;

                    bool okBytes = false;
                    const QByteArray newBytes = bytesToArray(text, &okBytes);
                    if (!okBytes || newBytes.isEmpty()) {
                        QMessageBox::warning(this, tr("String patch"), tr("Invalid hex bytes."));
                        return;
                    }

                    QFile f(m_fileContext->filePath());
                    if (!f.open(QIODevice::ReadWrite)) {
                        QMessageBox::warning(this, tr("String patch"), tr("Failed to open file for writing."));
                        return;
                    }
                    if (!f.seek(static_cast<qint64>(saddr))) {
                        QMessageBox::warning(this, tr("String patch"), tr("Failed to seek to 0x%1.").arg(QString::number(saddr, 16)));
                        return;
                    }
                    const qint64 wr = f.write(newBytes);
                    f.close();
                    if (wr != newBytes.size()) {
                        QMessageBox::warning(this, tr("String patch"), tr("Failed to write all bytes."));
                        return;
                    }

                    appendLog(QString("[hexpatch] wrote %1 bytes at string 0x%2")
                                  .arg(newBytes.size())
                                  .arg(QString::number(saddr, 16)));
                    if (m_running) cancelDisassembly();
                    startDisassembly();
                });
            }
            menu.addAction(tr("Show string bytes…"), [this, saddr]() {
                const QString str = m_stringByAddr.value(saddr);
                const QByteArray utf8 = str.toUtf8();
                const int want = utf8.size() + 1; // include '\0'

                QFile f(m_fileContext->filePath());
                if (!f.open(QIODevice::ReadOnly)) {
                    QMessageBox::warning(this, tr("String bytes"), tr("Failed to open file."));
                    return;
                }
                if (!f.seek(static_cast<qint64>(saddr))) {
                    QMessageBox::warning(this, tr("String bytes"), tr("Failed to seek to 0x%1.").arg(QString::number(saddr, 16)));
                    return;
                }
                const QByteArray data = f.read(want);
                f.close();

                QStringList hx;
                hx.reserve(data.size());
                for (unsigned char ch : data)
                    hx << QString("%1").arg(ch, 2, 16, QLatin1Char('0')).toUpper();

                QString ascii;
                ascii.reserve(data.size());
                for (unsigned char ch : data) {
                    if (ch >= 0x20 && ch < 0x7f) ascii += QChar(ch);
                    else if (ch == 0) ascii += "\\0";
                    else ascii += ".";
                }

                QMessageBox::information(
                    this,
                    tr("String bytes"),
                    tr("addr: 0x%1\nlen: %2 (+1 null)\n\nhex:\n%3\n\nascii:\n%4")
                        .arg(QString::number(saddr, 16))
                        .arg(utf8.size())
                        .arg(hx.join(' '))
                        .arg(ascii));
            });
        }

        menu.exec(m_disasmView->viewport()->mapToGlobal(pos));
    });

    m_topSplitter->addWidget(m_funcPanel);
    m_topSplitter->addWidget(m_disasmView);
    m_topSplitter->setStretchFactor(0, 0);
    m_topSplitter->setStretchFactor(1, 1);
    m_topSplitter->setSizes({220, 1000});

    m_stack->addWidget(m_topSplitter);
    m_splitter->addWidget(topWidget);

    /*── Bottom: log panel ─────────────────────────────────────────────────────*/
    m_logPanel = new QWidget(m_splitter);
    auto *logLayout = new QVBoxLayout(m_logPanel);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->setSpacing(0);

    /* Log header bar */
    QWidget *logHeader = new QWidget(m_logPanel);
    logHeader->setFixedHeight(24);
    auto *logHeaderLayout = new QHBoxLayout(logHeader);
    logHeaderLayout->setContentsMargins(8, 0, 8, 0);
    QLabel *logTitle = new QLabel(tr("Diagnostic Log"), logHeader);
    logHeaderLayout->addWidget(logTitle);
    logHeaderLayout->addStretch();
    QPushButton *clearBtn = new QPushButton(tr("Clear"), logHeader);
    clearBtn->setFixedHeight(18);
    clearBtn->setFixedWidth(44);
    logHeaderLayout->addWidget(clearBtn);
    logLayout->addWidget(logHeader);

    m_logView = new QPlainTextEdit(m_logPanel);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(5000);
    m_logView->setStyleSheet("QPlainTextEdit font-family: 'JetBrains Mono', 'Consolas', monospace; font-size: 11px;");
    logLayout->addWidget(m_logView, 1);
    m_splitter->addWidget(m_logPanel);

    /* Hide log panel initially; give table all the space */
    m_logPanel->setVisible(false);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);

    /*── Status bar ─────────────────────────────────────────────────────────────*/
    m_statusLabel = new QLabel(this);
    m_statusLabel->setFixedHeight(20);
    m_statusLabel->setStyleSheet("font-size: 11px; padding: 0 6px;");
    mainLayout->addWidget(m_statusLabel);

    showPlaceholder(tr("Press \"Disassemble\" to analyse the file"));
    m_statusLabel->setText(tr("Backend: %1").arg(
        AppSettings::disasmBackend() == AppSettings::DisasmBackend::Radare2 ? tr("radare2") : tr("objdump")));

    /*── Connections ───────────────────────────────────────────────────────────*/
    connect(m_runBtn,       &QPushButton::clicked,
            this,           &DisassemblerTab::startDisassembly);
    connect(m_cancelBtn,    &QPushButton::clicked,
            this,           &DisassemblerTab::cancelDisassembly);
    /* Debounced search: avoids rebuilding UI on every keystroke. */
    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(150);
    connect(m_searchDebounce, &QTimer::timeout, this, &DisassemblerTab::applyFilter);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        if (m_searchDebounce) m_searchDebounce->start();
    });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        if (!m_disasmView) return;
        const QString q = m_searchEdit->text().trimmed();
        if (q.isEmpty()) return;
        if (!m_disasmView->find(q)) {
            /* wrap-around */
            QTextCursor c = m_disasmView->textCursor();
            c.movePosition(QTextCursor::Start);
            m_disasmView->setTextCursor(c);
            m_disasmView->find(q);
        }
    });
    connect(m_sectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,           &DisassemblerTab::onSectionComboChanged);
    connect(m_logToggleBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_logPanel->setVisible(checked);
        if (checked) {
            /* Give log panel ~30% of space */
            int total = m_splitter->height();
            m_splitter->setSizes({total * 70 / 100, total * 30 / 100});
        }
    });
    connect(clearBtn, &QPushButton::clicked, m_logView, &QPlainTextEdit::clear);

    /* F5: re-analyze / disassemble again */
    auto *reanalyze = new QShortcut(QKeySequence(Qt::Key_F5), this);
    reanalyze->setContext(Qt::WidgetWithChildrenShortcut);
    connect(reanalyze, &QShortcut::activated, this, [this]() {
        if (m_running) cancelDisassembly();
        startDisassembly();
    });
}

bool DisassemblerTab::eventFilter(QObject *watched, QEvent *event)
{
    if (m_disasmView && watched == m_disasmView->viewport() && event) {
        if (event->type() == QEvent::ToolTip) {
            auto *helpEvent = static_cast<QHelpEvent *>(event);
            showInstructionHelpAt(helpEvent->pos(), false);
            return true;
        }
    }
    return ToolTab::eventFilter(watched, event);
}

void DisassemblerTab::showInstructionHelpAt(const QPoint &pos, bool forceByCursor)
{
    if (!m_disasmView)
        return;

    QTextCursor c = forceByCursor ? m_disasmView->textCursor() : m_disasmView->cursorForPosition(pos);
    c.select(QTextCursor::WordUnderCursor);
    const QString token = c.selectedText().trimmed();
    const QString line = c.block().text();

    QString tip = InstructionHelpService::instance().tooltipForToken(token, line);
    if (tip.isEmpty())
        tip = InstructionHelpService::instance().tooltipForLine(line);

    if (tip.isEmpty()) {
        QToolTip::hideText();
        return;
    }

    const QPoint globalPos = m_disasmView->viewport()->mapToGlobal(forceByCursor ? m_disasmView->cursorRect().bottomRight() : pos);
    QToolTip::showText(globalPos, tip, m_disasmView->viewport());
}

void DisassemblerTab::updateBackendUiHint()
{
    if (m_running) return;
    const QString backend = (AppSettings::disasmBackend() == AppSettings::DisasmBackend::Radare2) ? tr("radare2") : tr("objdump");
    m_statusLabel->setText(tr("Backend: %1").arg(backend));
}

void DisassemblerTab::onGlobalActionTriggered(const QString &actionName)
{
    if (actionName != "settingsChanged") return;
    appendLog("[settings] applied");
    updateBackendUiHint();
}

/*─────────────────────────────────────────────────────────────────────────────*/
void DisassemblerTab::startDisassembly()
{
    if (m_running) return;
    if (m_fileContext->filePath().isEmpty()) { showPlaceholder(tr("No file path set")); return; }

    // If radare2 backend is selected, ensure r2 is available. If not, prompt for path.
    if (AppSettings::disasmBackend() == AppSettings::DisasmBackend::Radare2) {
        QString r2 = AppSettings::radare2Path();
        if (r2.isEmpty())
            r2 = QStandardPaths::findExecutable("r2");

        if (r2.isEmpty()) {
            const QString picked = QFileDialog::getOpenFileName(
                this,
                tr("Select radare2 (r2) executable"),
                QString());
            if (picked.isEmpty()) {
                appendLog("[disasm] radare2 not configured; cancelled");
                showPlaceholder(tr("radare2 is not configured — set it in Settings"));
                return;
            }
            AppSettings::setRadare2Path(picked);
        }
    }

    m_sections.clear();
    m_functions.clear();
    m_strings.clear();
    m_stringByAddr.clear();
    m_lines.clear();
    m_visibleLineMap.clear();
    if (m_disasmView) m_disasmView->clear();
    if (m_funcList) m_funcList->clear();
    m_sectionCombo->blockSignals(true);
    m_sectionCombo->clear();
    m_sectionCombo->blockSignals(false);
    m_sectionCombo->setEnabled(false);
    m_searchEdit->setEnabled(false);
    m_currentSectionIndex = -1;

    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_statusLabel->setText(
        AppSettings::disasmBackend() == AppSettings::DisasmBackend::Radare2
            ? tr("Running radare2…")
            : tr("Running objdump…"));

    /* Auto-open log panel when disassembly starts so user sees progress */
    if (!m_logToggleBtn->isChecked()) {
        m_logToggleBtn->setChecked(true);
    }

    appendLog("=== Disassembly started: " +
              QDateTime::currentDateTime().toString("hh:mm:ss") + " ===");
    appendLog(QString("[disasm] backend: %1")
                  .arg(AppSettings::disasmBackend() == AppSettings::DisasmBackend::Radare2 ? "radare2" : "objdump"));

    setRunningState(true);
    showPlaceholder(tr("Disassembling…"));

    emit requestDisassembly(m_fileContext->filePath(), {});
}

void DisassemblerTab::cancelDisassembly()
{
    if (m_worker) m_worker->cancel();
}

/*─────────────────────────────────────────────────────────────────────────────*/
void DisassemblerTab::onLogLine(const QString &line)
{
    appendLog(line);
}

void DisassemblerTab::appendLog(const QString &line)
{
    m_logView->appendPlainText(line);
    /* Auto-scroll to bottom */
    QTextCursor c = m_logView->textCursor();
    c.movePosition(QTextCursor::End);
    m_logView->setTextCursor(c);
}

// ─────────────────────────────────────────────────────────────────────────────
void DisassemblerTab::onSectionFound(const DisasmSection &section)
{
    if (section.instructions.isEmpty()) return;
    const int secIndex = m_sections.size();
    m_sections.append(section);

    if (m_sections.size() == 1) {
        /* Listing UI is hosted inside m_topSplitter which is a direct child of m_stack. */
        m_stack->setCurrentWidget(m_topSplitter);
        populateSectionCombo();
    }

    if (m_currentSectionIndex == -1) {
        for (const DisasmInstruction &insn : section.instructions) {
            LineInfo li;
            li.sectionIndex = secIndex;
            li.address = insn.address;
            li.bytes = insn.bytes;
            li.mnemonic = insn.mnemonic;
            li.operands = insn.operands;
            li.fileOffset = insn.fileOffset;
            li.size = insn.size;
            li.addrL  = insn.address.toLower();
            li.bytesL = insn.bytes.toLower();
            li.mnemL  = insn.mnemonic.toLower();
            li.opsL   = insn.operands.toLower();
            m_lines.append(std::move(li));
        }
        /* Render incrementally in "All sections" mode for responsiveness. */
        applyFilter();
    }

    populateSectionCombo();

    int total = 0;
    for (const auto &s : m_sections) total += s.instructions.size();
    m_statusLabel->setText(
        tr("%1 section(s) · %2 instructions loaded…").arg(m_sections.size()).arg(total));
}

void DisassemblerTab::onFunctionsFound(const QVector<DisasmFunction> &funcs)
{
    setFunctionsList(funcs);
}

void DisassemblerTab::onStringsFound(const QVector<DisasmString> &strings)
{
    m_strings = strings;
    m_stringByAddr.clear();
    m_stringByAddr.reserve(strings.size());
    for (const auto &s : strings) {
        quint64 a = 0;
        if (!parseHexU64(s.address, &a)) continue;
        if (!m_stringByAddr.contains(a))
            m_stringByAddr.insert(a, s.value);
    }
    applyFilter();
}

void DisassemblerTab::setFunctionsList(const QVector<DisasmFunction> &funcs)
{
    m_functions = funcs;
    if (!m_funcList) return;
    m_funcList->clear();

    /* Sort by address (hex) when possible **
    QVector<DisasmFunction> sorted = m_functions;
    std::sort(sorted.begin(), sorted.end(), [](const DisasmFunction &a, const DisasmFunction &b) {
        quint64 va = 0, vb = 0;
        const bool oka = parseHexU64(a.address, &va);
        const bool okb = parseHexU64(b.address, &vb);
        if (oka && okb) return va < vb;
        return a.name < b.name;
    });

    for (const auto &f : sorted) {
        auto *it = new QListWidgetItem(QString("%1  %2").arg(f.address, f.name), m_funcList);
        it->setData(Qt::UserRole, f.address); 
    }
}

void DisassemblerTab::rebuildFunctionsFromLines()
{
    /* For objdump backend: function labels come as mnemonic "<name>" with empty bytes. **
    QVector<DisasmFunction> funcs;
    funcs.reserve(256);
    for (const auto &li : m_lines) {
        const QString m = li.mnemonic.trimmed();
        if (m.size() >= 3 && m.startsWith('<') && m.endsWith('>')) {
            DisasmFunction f;
            f.address = li.address.trimmed();
            f.name = m.mid(1, m.size() - 2);
            funcs.append(std::move(f));
        }
    }
    if (!funcs.isEmpty())
        setFunctionsList(funcs);
}

void DisassemblerTab::jumpToAddress(const QString &addr)
{
    if (!m_disasmView || addr.isEmpty()) return;

    /* 1. Convert target address to number for reliable compraison 
    quint64 targetAddr = 0;
    if (!parseHexU64(addr, &targetAddr)) return;

    int targetVisualLine = -1;

    /* 2. Search m_visibleMap for the index corresponding to this address. */
    /* This is much faster and more accurate than parsing text from QPlainTextEdit */
    for (int visLine = 0; visLine < m_visibleLineMap.size(); ++visLine) {
        int lineIdx = m_visibleLineMap[visLine];
        if (lineIdx < 0) continue; // Пропускаем строки-заголовки (где индекс -1)

        const LineInfo &li = m_lines[lineIdx];
        quint64 currentAddr = 0;
        if (parseHexU64(li.address, &currentAddr)) {
            if (currentAddr == targetAddr) {
                targetVisualLine = visLine;
                break;
            }
        }
    }

    /* 3. If line is found in the current view */
    if (targetVisualLine != -1) {
        QTextDocument *doc = m_disasmView->document();
        QTextBlock block = doc->findBlockByNumber(targetVisualLine);
        
        if (block.isValid()) {
            QTextCursor cursor(block);
            m_disasmView->setTextCursor(cursor);
            m_disasmView->centerCursor();

            // Visual jump highlighting
            QList<QTextEdit::ExtraSelection> extra;
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(m_disasmView->palette().highlight());
            sel.format.setForeground(m_disasmView->palette().highlightedText());
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            sel.cursor = cursor;
            extra.append(sel);
            m_disasmView->setExtraSelections(extra);
        }
    } else {
        /* If the address is not found (e.g., it is in another section currently hidden by a filter) */
        m_statusLabel->setText(tr("Address %1 is not visible in current view").arg(addr));
    }
}

void DisassemblerTab::onWorkerFinished()
{
    setRunningState(false);
    m_progressBar->setVisible(false);

    appendLog("=== Disassembly finished ===");

    if (m_sections.isEmpty()) {
        showPlaceholder(
            tr("No disassemblable sections found.\n"
               "Make sure the file is a supported binary (ELF, PE, Mach-O…)\n"
               "Check the Log panel for details."));
        m_statusLabel->clear();
        return;
    }

    int total = 0;
    for (const auto &s : m_sections) total += s.instructions.size();
    m_statusLabel->setText(
        tr("%1 section(s) · %2 instructions").arg(m_sections.size()).arg(total));

    m_sectionCombo->setEnabled(true);
    m_searchEdit->setEnabled(true);

    // If backend didn't provide functions (objdump), derive from labels.
    if (m_functions.isEmpty())
        rebuildFunctionsFromLines();
}

void DisassemblerTab::onWorkerError(const QString &msg)
{
    setRunningState(false);
    m_progressBar->setVisible(false);
    appendLog("[ERROR] " + msg);
    showPlaceholder(tr("objdump error — see Log panel for details"));
    m_statusLabel->setText(tr("Error"));
}

void DisassemblerTab::onProgressUpdated(int percent)
{
    m_progressBar->setValue(percent);
}

/*─────────────────────────────────────────────────────────────────────────────*/
void DisassemblerTab::setRunningState(bool running)
{
    m_running = running;
    m_runBtn->setVisible(!running);
    m_cancelBtn->setVisible(running);
}

void DisassemblerTab::showPlaceholder(const QString &msg)
{
    m_placeholderLbl->setText(msg);
    m_stack->setCurrentWidget(m_placeholderLbl);
}

void DisassemblerTab::populateSectionCombo()
{
    m_sectionCombo->blockSignals(true);
    int prev = m_sectionCombo->currentIndex();
    m_sectionCombo->clear();
    m_sectionCombo->addItem(tr("All sections"));
    for (const auto &s : m_sections)
        m_sectionCombo->addItem(s.name);
    m_sectionCombo->setCurrentIndex(prev >= 0 && prev < m_sectionCombo->count() ? prev : 0);
    m_sectionCombo->blockSignals(false);
}

void DisassemblerTab::onSectionComboChanged(int index)
{
    m_currentSectionIndex = (index <= 0) ? -1 : (index - 1);
    applyFilter();
}

void DisassemblerTab::onSearchTextChanged(const QString &)
{
    if (m_searchDebounce) {
        m_searchDebounce->start();
        return;
    }
    applyFilter();
}

void DisassemblerTab::applyFilter()
{
    if (m_sections.isEmpty() || !m_disasmView) return;

    const QString query = m_searchEdit->text().trimmed().toLower();
    const bool haveQuery = !query.isEmpty();

    m_visibleLineMap.clear();
    QStringList out;
    out.reserve(m_lines.size());

    /* Xash-Map of function names for fast header insertion */
    QHash<QString, QString> funcByAddr;
    for (const auto &f : m_functions) {
        funcByAddr.insert(f.address.trimmed().toLower(), f.name);
    }

    int shownLines = 0;

    for (int i = 0; i < m_lines.size(); ++i) {
        const LineInfo &li = m_lines[i];

        /* Filtering by section and search query */
        bool inSection = (m_currentSectionIndex < 0) || (li.sectionIndex == m_currentSectionIndex);
        if (!inSection) continue;

        if (haveQuery) {
            bool match = li.addrL.contains(query) || li.bytesL.contains(query) || 
                         li.mnemL.contains(query) || li.opsL.contains(query);
            if (!match) continue;
        }

        /* Function header insertion (IDA-style) */
        QString normAddr = li.address.trimmed().toLower();
        if (funcByAddr.contains(normAddr)) {
            out << QString("; --- FUNCTION: %1 (%2) ---").arg(funcByAddr.value(normAddr), li.address.trimmed());
            m_visibleLineMap << -1; 
        }

        /* Optimization: Merging consecutive "invalid" instructions into a single block */
        if (li.mnemonic.trimmed().compare("invalid", Qt::CaseInsensitive) == 0 && !li.bytes.isEmpty()) {
            quint64 curAddr = 0;
            if (parseHexU64(li.address, &curAddr)) {
                QString joinedBytes = normalizeBytes(li.bytes);
                quint64 nextExpected = curAddr + (joinedBytes.size() / 2);

                int j = i + 1;
                while (j < m_lines.size()) {
                    const LineInfo &next = m_lines[j];
                    if (next.sectionIndex != li.sectionIndex || 
                        next.mnemonic.trimmed().compare("invalid", Qt::CaseInsensitive) != 0) break;

                    quint64 nextAddr = 0;
                    if (!parseHexU64(next.address, &nextAddr) || nextAddr != nextExpected) break;

                    QString nb = normalizeBytes(next.bytes);
                    joinedBytes += nb;
                    nextExpected += (nb.size() / 2);
                    j++;
                }

                LineInfo coalesced = li;
                coalesced.bytes = joinedBytes;
                coalesced.operands.clear();
                
                out << formatLine(coalesced);
                m_visibleLineMap << i;
                shownLines++;
                i = j - 1; /* Skip processed lines */
                continue;
            }
        }

        /* Regular string */
        out << formatLine(li);
        m_visibleLineMap << i;
        shownLines++;
    }

    /* Update UI */
    m_disasmView->setPlainText(out.join('\n'));

    if (shownLines == 0 && haveQuery) {
        showPlaceholder(tr("No results for \"%1\"").arg(m_searchEdit->text()));
    } else {
        m_stack->setCurrentWidget(m_topSplitter);
    }
    
    m_statusLabel->setText(tr("%1 lines shown").arg(shownLines));
}

#include "moc_disassemblertab.cpp"
