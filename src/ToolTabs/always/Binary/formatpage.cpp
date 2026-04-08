#include "formatpage.h"

#include "core/FileDataBuffer.h"

// чтобы Qt создал vtable для FormatPage
FormatPage::~FormatPage() {}

void FormatPage::setSharedBuffer(FileDataBuffer* buffer)
{
    m_sharedBuffer = buffer;
    if (!m_sharedBuffer)
        return;

    QByteArray data = m_sharedBuffer->data();
    setPageData(data);
}
