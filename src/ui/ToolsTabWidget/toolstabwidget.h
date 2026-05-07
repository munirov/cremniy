#ifndef TOOLTABWIDGET_H
#define TOOLTABWIDGET_H

#include "core/modules/ModuleManager.h"
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
class TabBase;

class ToolsTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    ToolsTabWidget(QWidget *parent, QString path);
    TabBase* openToolTab(const QString& toolId, bool activate = true);
    int saveToFileCurrentTab(QString path);
    void setDataInTabs(QByteArray &data, int index = -1, int excluded_index = -1);

private:
    void loadStyle(QString path, QString name);
    void createAlwaysTabs();
    void updateCloseButtons();
    void createTab(const ModuleDescription<TabBase>& desc, bool isAlways = false, bool tabClosable = true);
    FileDataBuffer* m_sharedBuffer = nullptr;
    QString m_filePath;

public slots:
    void closeToolTab(int index);
    void saveCurrentTabData();
    void refreshDataAllTabs();

    void removeStar();
    void setupStar();

    void setWordWrapSlot(bool checked);
    void setTabReplaceSlot(bool checked);
    void setTabWidthSlot(int width);

    void openTabModule(ModuleDescription<TabBase> desc);

signals:
    void removeStarSignal();
    void setupStarSignal();
    void saveFileSignal();
    void statusBarInfoChanged(const QString& info);

    void setWordWrapSignal(bool checked);
    void setTabReplaceSignal(bool checked);
    void setTabWidthSignal(int width);

};

#endif // TOOLTABWIDGET_H
