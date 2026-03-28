#ifndef FILETAB_H
#define FILETAB_H

#include "toolstabwidget.h"
#include <QWidget>

class FileTab : public QWidget
{
    Q_OBJECT

private:
    ToolsTabWidget *m_tooltabWidget;

public:
    explicit FileTab(QWidget *parrent, QString path);
    QString filePath;

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
