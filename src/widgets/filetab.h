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

public:
    explicit FileTab(QWidget *parrent, QString path);
    QString filePath;
    bool isFileUnsaved() const { return m_modified; }

public slots:
    void removeStar();
    void setupStar();
    void saveFile();

signals:
    void removeStarSignal(FileTab* tab);
    void setupStarSignal(FileTab* tab);
    void saveFileSignal();

};

#endif // FILETAB_H
