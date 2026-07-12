#ifndef CODEEDITORTAB_H
#define CODEEDITORTAB_H

#include "libs/CodeEditor/include/widgets/CustomCodeEditor.h"
#include "core/modules/TabBase.h"
#include <QShortcut>
#include <QWidget>

class CodeEditorTab : public TabBase
{
    Q_OBJECT

private:
    CustomCodeEditor* m_codeEditorWidget;
    QWidget* m_overlayWidget;

    bool forceSetData = false;
    bool m_largeFileMode = false;
    bool m_updatingSelection = false;
    QShortcut* m_findShortcut = nullptr;
    QShortcut* m_findNextShortcut = nullptr;
    QShortcut* m_findPreviousShortcut = nullptr;
    QShortcut* m_goToLineShortcut = nullptr;
    QShortcut* m_replaceShortcut = nullptr;
    QShortcut* m_projectFindShortcut = nullptr;
    QString m_currentLang = "Plain Text";

    static QString detectLanguage(const QString& filePath);

public:
    explicit CodeEditorTab(QWidget *parent = nullptr);

    QIcon icon() const override { return QIcon(":/icons/code.svg"); };

    void setFileDataBuffer(FileDataBuffer* newFileDataBuffer) override;
    CustomCodeEditor* editor() const { return m_codeEditorWidget; }

signals:
    void switchHexViewTab();

protected slots:
    void onSelectionChanged(qint64 pos, qint64 length) override;
    void onDataChanged() override;

public slots:
    void setFile(QString filepath) override;
    void setTabData() override;
    void saveTabData() override;

    void setWordWrapSlot(bool checked) override;
    void setTabReplaceSlot(bool checked) override;
    void setTabWidthSlot(int width) override;

    void navigateToLine(int lineNumber, const QString& highlightText);
};

#endif // CODEEDITORTAB_H
