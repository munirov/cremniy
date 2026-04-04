#ifndef FILETAB_H
#define FILETAB_H

#include "toolstabwidget.h"
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

public slots:
    void removeStar();
    void setupStar();
    void saveFile();
    void navigateToCodeLine(int lineNumber, bool selectWholeLine = false);
    void navigateToSearchHit(int lineNumber, const QString &needle);

signals:
    void removeStarSignal(FileTab* tab);
    void setupStarSignal(FileTab* tab);
    void saveFileSignal();
    void pinnedChanged(FileTab* tab);

};

#endif // FILETAB_H
