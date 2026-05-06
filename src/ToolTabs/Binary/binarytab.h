#ifndef BINARYTAB_H
#define BINARYTAB_H

#include "core/ToolTab.h"
#include <QWidget>
#include <qfileinfo.h>
#include <qstackedwidget.h>

class BinaryTab : public ToolTab
{
    Q_OBJECT

private:

    QStackedWidget* pageView;
    bool m_updatingSelection = false; // Flag to prevent recursion
    bool m_syncingBufferData = false;

protected slots:
    // Selection change handler from the buffer
    void onSelectionChanged(qint64 pos, qint64 length) override;
    void onDataChanged() override;

public:
    explicit BinaryTab(FileDataBuffer* buffer, QWidget *parent = nullptr);

    QString toolName() const override { return "Binary"; };
    QIcon toolIcon() const override { return QIcon(":/icons/binary.png"); };

public slots:

    // From Parrent Class: ToolTab
    void setFile(QString filepath) override;
    void setTabData() override;
    void saveTabData() override;

    void pageModifyDataSlot();

};

#endif // BINARYTAB_H
