#ifndef FILETAB_H
#define FILETAB_H

#include "core/modules/ModuleManager.h"
#include "ui/ToolsTabWidget/toolstabwidget.h"
#include <QWidget>

class FileTab : public QWidget
{
    Q_OBJECT

private:
    ToolsTabWidget *m_tooltabWidget;
    bool m_modified = false;
    bool m_pinned = false;

public:
    explicit FileTab(QWidget *parrent, QString path);
    QString filePath;
    bool isFileUnsaved() const { return m_modified; }
    bool isPinned() const { return m_pinned; }
    void setPinned(bool pinned);
    ToolsTabWidget* toolsTabWidget() const { return m_tooltabWidget; }

public slots:
    void removeStar();
    void setupStar();
    void saveFile();

    void setWordWrapSlot(bool checked);
    void setTabReplaceSlot(bool checked);
    void setTabWidthSlot(int width);

signals:
    void removeStarSignal(FileTab* tab);
    void setupStarSignal(FileTab* tab);
    void saveFileSignal();
    void pinnedChanged(FileTab* tab);

    void setWordWrapSignal(bool checked);
    void setTabReplaceSignal(bool checked);
    void setTabWidthSignal(int width);

    void statusBarInfoChanged(const QString& info);

    void openTabModule(ModuleDescription<TabBase> desc);

};

#endif // FILETAB_H
