#ifndef FORMATPAGE_H
#define FORMATPAGE_H

#include <QWidget>
#include <qboxlayout.h>

class FileDataBuffer;

class FormatPage : public QWidget
{
    Q_OBJECT

protected:
    uint m_dataHash = 0;
    FileDataBuffer* m_sharedBuffer = nullptr;

public:
    explicit FormatPage(QWidget *parent = nullptr) : QWidget(parent) {};
    ~FormatPage();

    virtual QString pageName() const = 0;

    virtual void setPageData(QByteArray& data) = 0;
    virtual QByteArray getPageData() const = 0;
    virtual bool showFind() { return false; }
    virtual void setSharedBuffer(FileDataBuffer* buffer);
    
    // Установить выделение (pos - позиция байта, length - длина)
    virtual void setSelection(qint64 pos, qint64 length) = 0;

signals:
    void modifyData();
    void dataEqual();
    void selectionChanged(qint64 pos, qint64 length);
    void pageDataChanged(const QByteArray& data);


};

#endif // FORMATPAGE_H
