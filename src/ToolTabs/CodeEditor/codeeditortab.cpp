#include "codeeditortab.h"
#include "QCodeEditor.hpp"
#include <qboxlayout.h>
#include <qfileinfo.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qstackedlayout.h>
#include "filemanager.h"
#include "utils.h"

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

    // - - Create and Init Stacked Layout Widget - -

    auto stack = new QStackedLayout;
    stack->setStackingMode(QStackedLayout::StackAll);
    stack->addWidget(m_codeEditorWidget);
    stack->addWidget(m_overlayWidget);

    m_overlayWidget->hide();

    this->setLayout(stack);

    // - - Connects - -

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
                if (cursor.hasSelection()) {
                    int charStart = cursor.selectionStart();
                    int charEnd = cursor.selectionEnd();
                    
                    // Преобразуем позицию символа в позицию байта
                    QString text = m_codeEditorWidget->toPlainText();
                    QByteArray utf8Data = text.toUtf8();
                    
                    // Получаем байтовую позицию начала выделения
                    QString beforeSelection = text.left(charStart);
                    qint64 byteStart = beforeSelection.toUtf8().size();
                    
                    // Получаем длину выделения в байтах
                    QString selectedText = text.mid(charStart, charEnd - charStart);
                    qint64 byteLength = selectedText.toUtf8().size();
                    
                    // Уведомляем буфер о выделении
                    m_dataBuffer->setSelection(byteStart, byteLength);
                }
                
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
        if (pos < kUtf8Bom.size()) {
            length = qMax<qint64>(0, length - (kUtf8Bom.size() - pos));
            pos = kUtf8Bom.size();
        }
        pos -= qMin<qint64>(pos, static_cast<qint64>(kUtf8Bom.size()));
        data.remove(0, kUtf8Bom.size());
    }

    pos = qBound<qint64>(0, pos, data.size());
    length = qBound<qint64>(0, length, data.size() - pos);

    const QByteArray beforeSelection = data.left(pos);
    const QString beforeText = QString::fromUtf8(beforeSelection);
    const int charStart = beforeText.length();

    const QByteArray selectedBytes = data.mid(pos, length);
    const QString selectedText = QString::fromUtf8(selectedBytes);
    const int charLength = selectedText.length();

    QTextCursor cursor = m_codeEditorWidget->textCursor();
    cursor.setPosition(charStart);
    cursor.setPosition(charStart + charLength, QTextCursor::KeepAnchor);
    m_codeEditorWidget->setTextCursor(cursor);

    m_updatingSelection = false;
}
