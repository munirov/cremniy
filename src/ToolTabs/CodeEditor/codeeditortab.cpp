#include "codeeditortab.h"
#include "QCodeEditor.hpp"
#include <QStyle>
#include <qboxlayout.h>
#include <qfileinfo.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qstackedlayout.h>
#include "filemanager.h"
#include "utils.h"

#include <QLineEdit>
#include <QShortcut>
#include <QKeySequence>
#include <QTextCursor>
#include <QHBoxLayout>


#include "core/ToolTabFactory.h"

static bool registered = [](){
    ToolTabFactory::instance().registerTab("1", [](FileDataBuffer* buffer){
        return new CodeEditorTab(buffer);
    });
    return true;
}();

namespace {
const QByteArray kUtf8Bom("\xEF\xBB\xBF", 3);
}

CodeEditorTab::CodeEditorTab(FileDataBuffer* buffer, QWidget *parent)
    : ToolTab{buffer, parent}
{
    m_selectionSyncTimer = new QTimer(this);
    m_selectionSyncTimer->setSingleShot(true);
    connect(m_selectionSyncTimer, &QTimer::timeout, this, [this]() {
        applyBufferedSelection();
    });

    // - - Create "Code Editor" Page - -

    m_codeEditorWidget = new QCodeEditor(this);

    QTextOption opt = m_codeEditorWidget->document()->defaultTextOption();
    opt.setTabStopDistance(20);
    m_codeEditorWidget->document()->setDefaultTextOption(opt);

    m_codeEditorWidget->document()->markContentsDirty(0, m_codeEditorWidget->document()->characterCount());
    m_codeEditorWidget->viewport()->update();

    // - - Create "Binary File Detected" Page - -

    m_overlayWidget = new QWidget(this);

    auto overlayLayout = new QVBoxLayout(m_overlayWidget);
    overlayLayout->setAlignment(Qt::AlignCenter);

    QLabel* title = new QLabel("Binary file detected");
    title->setStyleSheet("color: white; font-size: 20px;");
    title->setAlignment(Qt::AlignCenter);
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    overlayLayout->addWidget(title);
    overlayLayout->addSpacing(15);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setAlignment(Qt::AlignCenter);

    QPushButton* anywayOpenBtn = new QPushButton("Open anyway");

    btnLayout->addWidget(anywayOpenBtn);
    overlayLayout->addLayout(btnLayout);

    // - - Create Search Bar Widget - -
    m_searchWidget = new QWidget(this);
    m_searchWidget->setObjectName("searchWidget");

    
    QHBoxLayout* searchLayout = new QHBoxLayout(m_searchWidget);
    searchLayout->setContentsMargins(6, 6, 6, 6);
    searchLayout->setSpacing(5);

    m_searchLineEdit = new QLineEdit(this);
    m_searchLineEdit->setPlaceholderText("Find...");
    
    m_findPrevBtn = new QPushButton("▲ Prev", this);
    m_findNextBtn = new QPushButton("▼ Next", this);
    m_closeSearchBtn = new QPushButton("✕", this);
    
    m_closeSearchBtn->setFixedWidth(30);

    searchLayout->addWidget(m_searchLineEdit);
    searchLayout->addWidget(m_findPrevBtn);
    searchLayout->addWidget(m_findNextBtn);
    searchLayout->addWidget(m_closeSearchBtn);
    
    m_searchWidget->hide();

    // - - Create and Init Stacked Layout Widget - -
    auto stack = new QStackedLayout;
    stack->setStackingMode(QStackedLayout::StackAll);
    stack->addWidget(m_codeEditorWidget);
    stack->addWidget(m_overlayWidget);

    m_overlayWidget->hide();

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addLayout(stack);
    mainLayout->addWidget(m_searchWidget); 

    this->setLayout(mainLayout);

    // - - Search Bar Connects & Shortcuts - -

    // Ctrl+F
    QShortcut* searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    searchShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(searchShortcut, &QShortcut::activated, this, &CodeEditorTab::showSearchBar);

    // Esc
    QShortcut* escapeShortcut = new QShortcut(QKeySequence("Esc"), m_searchWidget);
    escapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escapeShortcut, &QShortcut::activated, this, &CodeEditorTab::hideSearchBar);

    connect(m_closeSearchBtn, &QPushButton::clicked, this, &CodeEditorTab::hideSearchBar);

    connect(m_searchLineEdit, &QLineEdit::returnPressed, this, [this]() {
        performSearch(false);
    });

    connect(m_findNextBtn, &QPushButton::clicked, this, [this]() { performSearch(false); });
    connect(m_findPrevBtn, &QPushButton::clicked, this, [this]() { performSearch(true); });

    connect(m_searchLineEdit, &QLineEdit::textChanged, this, [this]() {
        performSearch(false);
    });


    // Trigger: Menu Bar: View->wordWrap - setWordWrapMode
    // connect(GlobalWidgetsManager::instance().get_IDEWindow_menuBar_view_wordWrap(),
    //         &QAction::changed,
    //         this, [this]{
    //             if (GlobalWidgetsManager::instance().get_IDEWindow_menuBar_view_wordWrap()->isChecked())
    //                 m_codeEditorWidget->setWordWrapMode(QTextOption::WordWrap);
    //             else
    //                 m_codeEditorWidget->setWordWrapMode(QTextOption::NoWrap);
    //         });

    // Clicked: Open File Anyway Button
    connect(anywayOpenBtn, &QPushButton::clicked, this, [this]{
        forceSetData = true;
        this->setTabData();
    });

    // ContentsChanged: синхронизируем рабочую копию буфера и dirty-state
    connect(m_codeEditorWidget->document(),
            &QTextDocument::contentsChanged,
            this,
            [this](){
                if (m_codeEditorWidget->m_ignoreModification || m_syncingBufferData)
                    return;

                const QByteArray data = editorDataWithBom();
                const QByteArray currentBufferData = m_dataBuffer->data();

                if (data == currentBufferData) {
                    m_codeEditorWidget->document()->setModified(m_dataBuffer->isModified());

                    if (m_dataBuffer->isModified()) {
                        setModifyIndicator(true);
                        emit modifyData();
                    } else {
                        setModifyIndicator(false);
                        emit dataEqual();
                    }
                    return;
                }

                m_syncingBufferData = true;
                m_dataBuffer->replaceData(data);
                m_syncingBufferData = false;

                if (m_dataBuffer->isModified()) {
                    setModifyIndicator(true);
                    emit modifyData();
                } else {
                    setModifyIndicator(false);
                    emit dataEqual();
                }
            });

    // SelectionChanged: уведомляем буфер о выделении
    connect(m_codeEditorWidget, &QPlainTextEdit::selectionChanged,
            this, [this](){
                if (m_updatingSelection) return; // Предотвращаем рекурсию
                
                // Устанавливаем флаг перед отправкой сигнала
                m_updatingSelection = true;
                
                QTextCursor cursor = m_codeEditorWidget->textCursor();
                const int charStart = cursor.hasSelection() ? cursor.selectionStart() : cursor.position();
                const int charEnd = cursor.hasSelection() ? cursor.selectionEnd() : cursor.position();
                
                // Преобразуем позицию символа в позицию байта (с учетом CRLF -> LF в редакторе)
                const QString text = m_codeEditorWidget->toPlainText();
                const qint64 normStart = text.left(charStart).toUtf8().size();
                const qint64 normEnd = normStart + text.mid(charStart, charEnd - charStart).toUtf8().size();

                QByteArray rawData = m_dataBuffer->data();
                qint64 baseOffset = 0;
                if (m_hasUtf8Bom && rawData.startsWith(kUtf8Bom)) {
                    rawData.remove(0, kUtf8Bom.size());
                    baseOffset = kUtf8Bom.size();
                }

                qint64 rawStart = -1;
                qint64 rawEnd = -1;
                qint64 rawIndex = 0;
                qint64 normIndex = 0;

                if (normStart == 0)
                    rawStart = 0;
                if (normEnd == 0)
                    rawEnd = 0;

                while (rawIndex < rawData.size()) {
                    if (normIndex == normStart && rawStart < 0)
                        rawStart = rawIndex;
                    if (normIndex == normEnd && rawEnd < 0)
                        rawEnd = rawIndex;

                    if (rawData[rawIndex] == '\r') {
                        if (rawIndex + 1 < rawData.size() && rawData[rawIndex + 1] == '\n') {
                            rawIndex += 2;
                        } else {
                            rawIndex += 1;
                        }
                        normIndex += 1;
                        continue;
                    }

                    rawIndex += 1;
                    normIndex += 1;
                }

                if (rawStart < 0)
                    rawStart = rawData.size();
                if (rawEnd < 0)
                    rawEnd = rawData.size();

                rawStart += baseOffset;
                rawEnd += baseOffset;

                const qint64 byteStart = rawStart;
                const qint64 byteLength = qMax<qint64>(0, rawEnd - rawStart);

                // Уведомляем буфер о выделении
                m_dataBuffer->setSelection(byteStart, byteLength);
                
                m_updatingSelection = false;
            });

}

// - - override functions - -

// - public slots -

void CodeEditorTab::setFile(QString filepath){
    m_fileContext = new FileContext(filepath);
    QFileInfo fileInfo(filepath);
    QString ext = (fileInfo.suffix()).toLower();
    m_codeEditorWidget->setFileExt(ext);
}

void CodeEditorTab::setTabData(){

    qDebug() << "CodeEditorTab: setTabData";

    const QByteArray probeData = m_dataBuffer->read(0, 4096);

    if (isBinary(probeData) && !forceSetData){
        m_codeEditorWidget->hide();
        m_overlayWidget->show();
    }
    else{
        QByteArray data = m_dataBuffer->data();
        m_hasUtf8Bom = data.startsWith(kUtf8Bom);
        if (m_hasUtf8Bom)
            data.remove(0, kUtf8Bom.size());

        m_codeEditorWidget->show();
        m_overlayWidget->hide();
        m_syncingBufferData = true;
        m_codeEditorWidget->setBData(data);
        m_syncingBufferData = false;
        forceSetData = false;
    }

    if (m_dataBuffer->isModified()) {
        setModifyIndicator(true);
        emit modifyData();
    } else {
        setModifyIndicator(false);
        emit dataEqual();
    }
}

void CodeEditorTab::onDataChanged()
{
    if (m_syncingBufferData)
        return;

    setTabData();
}

void CodeEditorTab::onSelectionChanged(qint64 pos, qint64 length)
{
    if (m_updatingSelection)
        return;

    m_pendingSelectionPos = pos;
    m_pendingSelectionLength = length;
    m_selectionSyncTimer->start(35);
}

void CodeEditorTab::saveTabData() {
    qDebug() << "CodeEditorTab: saveTabData";

    if (!m_codeEditorWidget->m_ignoreModification && !m_syncingBufferData)
        m_dataBuffer->replaceData(editorDataWithBom());

    if (!m_dataBuffer->isModified())
        return;

    if (!m_dataBuffer->saveToFile(m_fileContext->filePath()))
        return;

    m_codeEditorWidget->document()->setModified(false);

    setModifyIndicator(false);
    emit dataEqual();
    emit refreshDataAllTabsSignal();
}


void CodeEditorTab::showSearchBar()
{
    // Если открыта панель "Binary File", поиск не нужен
    if (!m_overlayWidget->isHidden()) return; 

    m_searchWidget->show();
    m_searchLineEdit->setFocus();
    m_searchLineEdit->selectAll(); // Выделяем текст, если там уже что-то было
}

void CodeEditorTab::hideSearchBar()
{
    m_searchWidget->hide();
    m_codeEditorWidget->setFocus(); // Возвращаем фокус в редактор
}

void CodeEditorTab::performSearch(bool backward)
{
    QString query = m_searchLineEdit->text();
    if (query.isEmpty()) {
        m_searchLineEdit->setStyleSheet(""); // Сброс стиля
        return;
    }

    QTextDocument::FindFlags flags;
    if (backward) flags |= QTextDocument::FindBackward;
    
    // Выполняем поиск
    bool found = m_codeEditorWidget->find(query, flags);

    if (!found) {
        QTextCursor cursor = m_codeEditorWidget->textCursor();
        cursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        m_codeEditorWidget->setTextCursor(cursor);
        
        found = m_codeEditorWidget->find(query, flags);
    }

    if (!found) {
        m_searchLineEdit->setProperty("state", "error");
    } else {
        m_searchLineEdit->setProperty("state", "normal");
    }
    // Обновляем виджет, чтобы Qt применил новые стили из QSS
    m_searchLineEdit->style()->unpolish(m_searchLineEdit);
    m_searchLineEdit->style()->polish(m_searchLineEdit);
}

QByteArray CodeEditorTab::editorDataWithBom() const
{
    QByteArray data = m_codeEditorWidget->getBData();
    if (m_hasUtf8Bom)
        data.prepend(kUtf8Bom);
    return data;
}

void CodeEditorTab::applyBufferedSelection()
{
    if (m_updatingSelection)
        return;

    m_updatingSelection = true;

    QByteArray data = m_dataBuffer->data();
    qint64 pos = m_pendingSelectionPos;
    qint64 length = m_pendingSelectionLength;

    if (m_hasUtf8Bom && data.startsWith(kUtf8Bom)) {
        data.remove(0, kUtf8Bom.size());
        pos = qMax<qint64>(0, pos - kUtf8Bom.size());
        length = qBound<qint64>(0, length, data.size() - pos);
    }

    pos = qBound<qint64>(0, pos, data.size());
    length = qBound<qint64>(0, length, data.size() - pos);

    // Map raw byte offsets to the normalized text representation used by QPlainTextEdit.
    // QPlainTextEdit collapses CRLF and CR to a single '\n', so we must mirror that.
    const qint64 rawEnd = pos + length;
    qint64 normPos = -1;
    qint64 normEnd = -1;
    qint64 rawIndex = 0;
    qint64 normIndex = 0;

    if (pos == 0)
        normPos = 0;
    if (rawEnd == 0)
        normEnd = 0;

    while (rawIndex < data.size()) {
        if (rawIndex == pos && normPos < 0)
            normPos = normIndex;
        if (rawIndex == rawEnd && normEnd < 0)
            normEnd = normIndex;

        if (data[rawIndex] == '\r') {
            if (rawIndex + 1 < data.size() && data[rawIndex + 1] == '\n') {
                rawIndex += 2;
            } else {
                rawIndex += 1;
            }
            normIndex += 1;
            continue;
        }

        rawIndex += 1;
        normIndex += 1;
    }

    if (normPos < 0)
        normPos = normIndex;
    if (normEnd < 0)
        normEnd = normIndex;

    qint64 normLength = qMax<qint64>(0, normEnd - normPos);

    QByteArray normalized;
    normalized.reserve(data.size());
    for (qint64 i = 0; i < data.size(); ++i) {
        if (data[i] == '\r') {
            if (i + 1 < data.size() && data[i + 1] == '\n')
                ++i;
            normalized.append('\n');
        } else {
            normalized.append(data[i]);
        }
    }

    normPos = qBound<qint64>(0, normPos, normalized.size());
    normLength = qBound<qint64>(0, normLength, normalized.size() - normPos);

    const QByteArray beforeSelection = normalized.left(normPos);
    const QString beforeText = QString::fromUtf8(beforeSelection);
    const int charStart = beforeText.length();

    const QByteArray selectedBytes = normalized.mid(normPos, normLength);
    const QString selectedText = QString::fromUtf8(selectedBytes);
    const int charLength = selectedText.length();

    QTextCursor cursor = m_codeEditorWidget->textCursor();
    cursor.setPosition(charStart);
    cursor.setPosition(charStart + charLength, QTextCursor::KeepAnchor);
    m_codeEditorWidget->setTextCursor(cursor);

    m_updatingSelection = false;

}
