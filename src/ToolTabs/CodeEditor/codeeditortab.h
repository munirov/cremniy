#ifndef CODEEDITORTAB_H
#define CODEEDITORTAB_H

#include "QCodeEditor/include/QCodeEditor.hpp"
#include "core/ToolTab.h"
#include <QWidget>
#include <QTimer>
#include <qfileinfo.h>
#include <qlabel.h>

class CodeEditorTab : public ToolTab
{
    Q_OBJECT

private:
    /**
     * @brief Widget of code editor
    */
    QCodeEditor* m_codeEditorWidget;
    QWidget* m_searchWidget;
    class QLineEdit* m_searchLineEdit;
    class QPushButton* m_findNextBtn;
    class QPushButton* m_findPrevBtn;
    class QPushButton* m_closeSearchBtn;

    /**
     * @brief Main page widget "Binary File Detected"
    */
    QWidget* m_overlayWidget;

    /**
     * @brief Data force-set flag 
     *
     * Used when the user clicks the "Open Anyway" button on the "Binary File Detected" page
    */
    bool forceSetData = false;
    bool m_hasUtf8Bom = false;
    
    /**
     * @brief Recursion prevention flag for selection update
    */
    bool m_updatingSelection = false;
    bool m_syncingBufferData = false;
    QTimer* m_selectionSyncTimer = nullptr;
    qint64 m_pendingSelectionPos = -1;
    qint64 m_pendingSelectionLength = 0;

    QByteArray editorDataWithBom() const;
    void applyBufferedSelection();

    /**
     * @brief Perfor search
     * @param Backward: Search in the reverse direction (backwards)
     */
    void performSearch(bool backward = false);

public:
    explicit CodeEditorTab(FileDataBuffer* buffer, QWidget *parent = nullptr);

    QString toolName() const override { return "Code"; };
    QIcon toolIcon() const override { return QIcon(":/icons/code.png"); };

signals:

    /**
     * @brief Switch to the "Hex View" tab 
     *
     * Used when the "Open in HexView" button is clicked on the "Binary File Detected" page
    */
    void switchHexViewTab();

protected slots:
    /* Selection change handler from buffer */
    void onSelectionChanged(qint64 pos, qint64 length) override;
    void onDataChanged() override;

public slots:

    /* From Parrent Class: ToolTab */
    void setFile(QString filepath) override;
    void setTabData() override;
    void saveTabData() override;
    void showSearchBar();
    void hideSearchBar();

};

#endif // CODEEDITORTAB_H
