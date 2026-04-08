#ifndef TOOLTABWIDGET_H
#define TOOLTABWIDGET_H

#include <QByteArray>
#include <QString>
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
class ToolTab;

class ToolsTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    ToolsTabWidget(QWidget *parent, QString path);
    ToolTab* openToolTab(const QString& toolId, bool activate = true);
    int saveToFileCurrentTab(QString path);
    void setDataInTabs(QByteArray &data, int index = -1, int excluded_index = -1);

private:
    void loadStyle(QString path, QString name);
    ToolTab* findToolTab(const QString& toolId) const;
    ToolTab* createToolTab(const QString& toolId);
    void updateCloseButtons();
    FileDataBuffer* m_sharedBuffer = nullptr;
    QString m_filePath;

public slots:
    void closeToolTab(int index);
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
