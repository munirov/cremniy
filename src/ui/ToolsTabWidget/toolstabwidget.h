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
class CodeEditorTab;

class ToolsTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    ToolsTabWidget(QWidget *parent, QString path);
    TabBase* openToolTab(const QString& toolId, bool activate = true);
    int saveToFileCurrentTab(QString path);
    void setDataInTabs(QByteArray &data, int index = -1, int excluded_index = -1);
    FileDataBuffer* sharedBuffer() const { return m_sharedBuffer; }
    bool gitBlameEnabled() const;
    CodeEditorTab* codeEditorTab(bool activate = false);

    /**
     * @brief Reload the shared buffer contents from the file on disk
     *
     * Re-reads the file into the shared buffer (discarding any in-editor
     * state) and refreshes every tool tab. Used by the "sync with disk"
     * workflow when a file was changed outside the editor.
     *
     * @return true on success, false if the file cannot be opened anymore
     */
    bool reloadFromDisk();

private:
    void loadStyle(QString path, QString name);
    void createAlwaysTabs();
    void updateCloseButtons();
    void createTab(const ModuleDescription<TabBase>& desc, bool isAlways = false, bool tabClosable = true);
    void connectGitIntegration(TabBase* tab);
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
    void setGitBlameSlot(bool checked);

    void openTabModule(ModuleDescription<TabBase> desc);

signals:
    void removeStarSignal();
    void setupStarSignal();
    void saveFileSignal();
    void statusBarInfoChanged(const QString& info);

    void setWordWrapSignal(bool checked);
    void setTabReplaceSignal(bool checked);
    void setTabWidthSignal(int width);
    void setGitBlameSignal(bool checked);
    void gitBlameEnabledChanged(bool enabled);

};

#endif // TOOLTABWIDGET_H
