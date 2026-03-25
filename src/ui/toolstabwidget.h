#ifndef TOOLTABWIDGET_H
#define TOOLTABWIDGET_H

#include <QTabWidget>

class QVBoxLayout;
class QSyntaxStyle;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QCompleter;
class QStyleSyntaxHighlighter;
class QCodeEditor;
class FileDataBuffer;

class ToolsTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    ToolsTabWidget(QWidget *parent, QString path);
    int saveToFileCurrentTab(QString path);
    void setDataInTabs(QByteArray &data, int index = -1, int excluded_index = -1);

private:
    void loadStyle(QString path, QString name);
    FileDataBuffer* m_sharedBuffer = nullptr;

public slots:
    void saveCurrentTabData();
    void refreshDataAllTabs();

    void removeStar();
    void setupStar();

signals:
    void removeStarSignal();
    void setupStarSignal();
    void saveFileSignal();

};

#endif // TOOLTABWIDGET_H
