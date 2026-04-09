#ifndef TOOLTABWIDGET_H
#define TOOLTABWIDGET_H

#include "core/ToolStatusState.h"
#include <QByteArray>
#include <QString>
#include <QTabWidget>
#include <QMetaObject>

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
    ToolStatusState currentStatusState() const;
    ToolTab* openToolTab(const QString& toolId, bool activate = true);
    int saveToFileCurrentTab(QString path);
    void setDataInTabs(QByteArray &data, int index = -1, int excluded_index = -1);

private:
    void loadStyle(QString path, QString name);
    ToolTab* findToolTab(const QString& toolId) const;
    ToolTab* createToolTab(const QString& toolId);
    void setActiveToolTab(ToolTab* tab);
    void updateCloseButtons();
    void updateActiveStatusState(const ToolStatusState& state);
    FileDataBuffer* m_sharedBuffer = nullptr;
    QString m_filePath;
    ToolTab* m_activeToolTab = nullptr;
    ToolStatusState m_activeStatusState = {"No tool selected", "", ""};
    QMetaObject::Connection m_activeStatusConnection;

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
    void activeStatusStateChanged(const ToolStatusState& state);

};

#endif // TOOLTABWIDGET_H
