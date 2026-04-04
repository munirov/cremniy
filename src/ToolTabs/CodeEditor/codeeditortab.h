#ifndef CODEEDITORTAB_H
#define CODEEDITORTAB_H

#include "widgets/CustomCodeEditor.h"
#include "core/ToolTab.h"
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QWidget>

class CodeEditorTab : public ToolTab
{
    Q_OBJECT

private:
    /**
     * @brief Виджет редактора кода
    */
    CustomCodeEditor* m_codeEditorWidget;

    /**
     * @brief Главный виджет страницы "Binary File Detected"
    */
    QWidget* m_overlayWidget;
    QWidget* m_searchBar = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QLabel* m_searchStatusLabel = nullptr;
    QPushButton* m_searchPrevButton = nullptr;
    QPushButton* m_searchNextButton = nullptr;
    QPushButton* m_searchCloseButton = nullptr;
    QCheckBox* m_matchCaseCheckBox = nullptr;

    /**
     * @brief Флаг принудительной установки данных
     *
     * Используется при нажатии пользователем на кнопку "Open Anyway" на странице "Binary File Detected"
    */
    bool forceSetData = false;
    bool m_largeFileMode = false;
    bool m_updatingSelection = false;
    QString m_lastSearchText;
    QShortcut* m_findShortcut = nullptr;
    QShortcut* m_findNextShortcut = nullptr;
    QShortcut* m_findPreviousShortcut = nullptr;
    QShortcut* m_goToLineShortcut = nullptr;

    void openFindDialog();
    void findNext(bool forward = true);
    void openGoToLineDialog();
    void updateSearchUi();
    void closeSearchBar();

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

};

#endif // CODEEDITORTAB_H
