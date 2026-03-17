#ifndef FILETAB_H
#define FILETAB_H

#include "tooltabwidget.h"
#include <QWidget>

class FileTab : public QWidget
{
    Q_OBJECT

private:
    ToolTabWidget *m_tooltabWidget;

public:
    explicit FileTab(QWidget *parrent, QString path);
    QString filePath;
    void saveFile();
    void openFile(int index = -1, int excluded_index = -1);   

public slots:
    void giveData(int index);

    void removeStar();
    void setupStar();

signals:
    void removeStarSignal(FileTab* tab);
    void setupStarSignal(FileTab* tab);

};

#endif // FILETAB_H
