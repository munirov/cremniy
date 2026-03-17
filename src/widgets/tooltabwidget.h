#ifndef TOOLTABWIDGET_H
#define TOOLTABWIDGET_H

#include "codeeditortab.h"
#include "disassemblertab.h"
#include "hexviewtab.h"
#include <QTabWidget>

class QVBoxLayout;
class QSyntaxStyle;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QCompleter;
class QStyleSyntaxHighlighter;
class QCodeEditor;

class ToolTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    ToolTabWidget(QWidget *parent, QString path);
    int saveToFileCurrentTab(QString path);
    void setDataInTabs(QByteArray &data, int index = -1, int excluded_index = -1);

private:
    void loadStyle(QString path, QString name);

    CodeEditorTab* m_codeEditorTab;
    HexViewTab* m_hexViewTab;
    DisassemblerTab* m_disassemblerTab;

public slots:
    void giveData();
    void setHexViewTab();

    void removeStar();
    void setupStar(bool modified);

signals:
    void askData(int index);

    void removeStarSignal();
    void setupStarSignal();

};

#endif // TOOLTABWIDGET_H
