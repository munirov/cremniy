#include "rawpage.h"
#include "formatpagefactory.h"

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

    /* Monitor hex view selection changes */
    connect(m_hexViewWidget->hexCursor(), &QHexCursor::positionChanged,
            this, [this](){
                /* if (m_hexViewWidget->hexCursor()->hasSelection()) { */
                    qint64 start = m_hexViewWidget->hexCursor()->selectionStartOffset();
                    qint64 length = m_hexViewWidget->hexCursor()->selectionLength();
                    emit selectionChanged(start, length);
                /* } */
            });

}

void RAWPage::setPageData(QByteArray& data) {
    m_hexViewWidget->setBData(data);
    m_dataHash = qHash(data, 0);
    emit dataEqual();
}

QByteArray RAWPage::getPageData() const {
    return m_hexViewWidget->getBData();
}

void RAWPage::setSelection(qint64 pos, qint64 length) {
    /* Set selection in Hex View */
    m_hexViewWidget->setSelectFromFormatPage(true);
    m_hexViewWidget->hexCursor()->setSelectFromFormatPage(true);
    m_hexViewWidget->hexCursor()->move(pos);
    m_hexViewWidget->hexCursor()->selectSize(length);

}

