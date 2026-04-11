#ifndef BINARYTAB_H
#define BINARYTAB_H

#include "ui/ToolsTabWidget/ToolTab.h"
#include <QShortcut>
#include <QWidget>
#include <QIcon>
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

    void setWordWrapSlot(bool checked) override {};
    void setTabReplaceSlot(bool checked) override {};
    void setTabWidthSlot(int width) override {};

};

#endif // BINARYTAB_H
