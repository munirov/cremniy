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
     * @brief Виджет редактора кода
    */
    QCodeEditor* m_codeEditorWidget;
    QWidget* m_searchWidget;
    class QLineEdit* m_searchLineEdit;
    class QPushButton* m_findNextBtn;
    class QPushButton* m_findPrevBtn;
    class QPushButton* m_closeSearchBtn;

    /**
     * @brief Главный виджет страницы "Binary File Detected"
    */
    QWidget* m_overlayWidget;

    /**
     * @brief Флаг принудительной установки данных
     *
     * Используется при нажатии пользователем на кнопку "Open Anyway" на странице "Binary File Detected"
    */
    bool forceSetData = false;
    bool m_hasUtf8Bom = false;
    
    /**
     * @brief Флаг для предотвращения рекурсии при обновлении выделения
    */
    bool m_updatingSelection = false;
    bool m_syncingBufferData = false;
    QTimer* m_selectionSyncTimer = nullptr;
    qint64 m_pendingSelectionPos = -1;
    qint64 m_pendingSelectionLength = 0;

    QByteArray editorDataWithBom() const;
    void applyBufferedSelection();

    /**
     * @brief Выполнить поиск
     * @param backward Искать в обратном направлении (назад)
     */
    void performSearch(bool backward = false);

public:
    explicit CodeEditorTab(FileDataBuffer* buffer, QWidget *parent = nullptr);

    QString toolName() const override { return "Code"; };
    QIcon toolIcon() const override { return QIcon(":/icons/code.png"); };

signals:

    /**
     * @brief Переключить на вкладку "Hex View"
     *
     * Используется при нажатии на кнопку "Open in HexView" на странице "Binary File Detected"
    */
    void switchHexViewTab();

protected slots:
    // Обработчик изменения выделения из буфера
    void onSelectionChanged(qint64 pos, qint64 length) override;
    void onDataChanged() override;

public slots:

    // From Parrent Class: ToolTab
    void setFile(QString filepath) override;
    void setTabData() override;
    void saveTabData() override;
    void showSearchBar();
    void hideSearchBar();

};

#endif // CODEEDITORTAB_H
