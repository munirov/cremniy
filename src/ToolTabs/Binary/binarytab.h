#ifndef BINARYTAB_H
#define BINARYTAB_H

#include "core/ToolTab.h"
#include <QShortcut>
#include <QWidget>
#include <qfileinfo.h>
#include <qstackedwidget.h>

class BinaryTab : public ToolTab
{
    Q_OBJECT

private:

    QStackedWidget* pageView;
    bool m_updatingSelection = false; // Флаг для предотвращения рекурсии
    bool m_syncingBufferData = false;
    bool m_pageDataDirty = true;
    QShortcut* m_findShortcut = nullptr;

    void openFindDialog();

protected slots:
    // Обработчик изменения выделения из буфера
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
