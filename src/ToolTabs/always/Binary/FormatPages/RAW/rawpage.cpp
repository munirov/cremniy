#include "rawpage.h"
#include "formatpagefactory.h"
#include "core/FileDataBuffer.h"

static bool registered = [](){
    FormatPageFactory::instance().registerPage("1", [](){
        return new RAWPage();
    });
    return true;
}();

RAWPage::RAWPage(QWidget *parent)
    : FormatPage(parent){

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);

    m_hexViewWidget = new QHexView(this);
    layout->addWidget(m_hexViewWidget);

    connect(m_hexViewWidget->hexDocument(),
            &QHexDocument::changed,
            this,
            [this](){
                if (m_sharedBuffer) {
                    if (m_sharedBuffer->isModified())
                        emit modifyData();
                    else
                        emit dataEqual();
                    return;
                }

                QByteArray data = m_hexViewWidget->getBData();
                uint newDataHash = qHash(data, 0);
                if (m_dataHash == newDataHash) {
                    emit dataEqual();
                }
                else{
                    if (!m_hexViewWidget->m_ignoreModification) {
                        emit pageDataChanged(data);
                        emit modifyData();
                    }
                }
            });

    // Отслеживаем изменение выделения в hex view
    connect(m_hexViewWidget->hexCursor(), &QHexCursor::positionChanged,
            this, [this](){
                if (m_hexViewWidget->hexCursor()->getSelectFromFormatPage()) {
                    m_hexViewWidget->hexCursor()->setSelectFromFormatPage(false);
                    m_hexViewWidget->setSelectFromFormatPage(false);
                    return;
                }

                if (m_hexViewWidget->hexCursor()->hasSelection()) {
                    qint64 start = m_hexViewWidget->hexCursor()->selectionStartOffset();
                    qint64 length = m_hexViewWidget->hexCursor()->selectionLength();
                    emit selectionChanged(start, length);
                } else {
                    // A single click in hex view focuses 1 byte visually. It should transfer as a 1 byte selection to Code Editor.
                    emit selectionChanged(m_hexViewWidget->hexCursor()->offset(), 1);
                }
            });

}

qint64 RAWPage::currentOffset() const
{
    return m_hexViewWidget->hexCursor()->offset();
}

qint64 RAWPage::currentSelectionStartOffset() const
{
    return m_hexViewWidget->hexCursor()->selectionStartOffset();
}

qint64 RAWPage::currentSelectionLength() const
{
    return m_hexViewWidget->hexCursor()->selectionLength();
}

qint64 RAWPage::dataSize() const
{
    if (m_sharedBuffer)
        return m_sharedBuffer->size();

    return m_hexViewWidget->getBData().size();
}

ToolStatusState RAWPage::statusState() const
{
    const qint64 size = dataSize();
    if (size <= 0) {
        return {"No data", "", "RAW"};
    }

    qint64 offset = currentOffset();
    if (currentSelectionLength() > 0) {
        offset = currentSelectionStartOffset();
    }

    offset = qBound<qint64>(0, offset, size - 1);

    const QString address = QString("0x%1")
        .arg(static_cast<qulonglong>(offset), 0, 16)
        .toUpper();

    return {
        QString("Address %1").arg(address),
        QString("Byte %1").arg(offset + 1),
        "RAW"
    };
}

void RAWPage::setPageData(QByteArray& data) {
    m_sharedBuffer = nullptr;
    m_hexViewWidget->setBData(data);
    m_dataHash = qHash(data, 0);
    emit dataEqual();
}

QByteArray RAWPage::getPageData() const {
    return m_hexViewWidget->getBData();
}

void RAWPage::setSelection(qint64 pos, qint64 length) {
    // Устанавливаем выделение в hex view
    m_hexViewWidget->setSelectFromFormatPage(true);
    m_hexViewWidget->hexCursor()->setSelectFromFormatPage(true);
    m_hexViewWidget->hexCursor()->move(pos);
    m_hexViewWidget->hexCursor()->selectSize(length);
}

bool RAWPage::showFind()
{
    m_hexViewWidget->showFind();
    return true;
}

void RAWPage::setSharedBuffer(FileDataBuffer* buffer)
{
    if (!buffer || m_sharedBuffer == buffer)
        return;

    m_sharedBuffer = buffer;
    m_hexViewWidget->setSelectFromFormatPage(true);
    m_hexViewWidget->hexCursor()->setSelectFromFormatPage(true);
    m_hexViewWidget->setSharedBuffer(buffer);
    m_dataHash = buffer->currentHash();
    emit dataEqual();
}
