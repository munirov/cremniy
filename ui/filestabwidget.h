#ifndef FILESTABWIDGET_H
#define FILESTABWIDGET_H

#include <filetab.h>
#include <QTabWidget>

class FilesTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    FilesTabWidget(QWidget *parent = nullptr);

    void tabSelect(int index);
    void openFile(QString fullPath, QString fileName);

public slots:
    void removeStar(FileTab* tab);
    void setupStar(FileTab* tab);
    void saveFileSlot();

};

#endif // FILESTABWIDGET_H
