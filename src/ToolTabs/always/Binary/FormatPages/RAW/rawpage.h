#ifndef RAWPAGE_H
#define RAWPAGE_H

#include "QHexView/qhexview.h"
#include "formatpage.h"

class RAWPage : public FormatPage
{ Q_OBJECT

private:
    QHexView* m_hexViewWidget;

public:
    explicit RAWPage(QWidget *parent = nullptr);

    QString pageName() const override { return "RAW"; }
    ToolStatusState statusState() const override;
    qint64 currentOffset() const;
    qint64 currentSelectionStartOffset() const;
    qint64 currentSelectionLength() const;
    qint64 dataSize() const;

    void setPageData(QByteArray& data) override;
    QByteArray getPageData() const override;
    void setSelection(qint64 pos, qint64 length) override;
    bool showFind() override;
    void setSharedBuffer(FileDataBuffer* buffer) override;

};

#endif // RAWPAGE_H
