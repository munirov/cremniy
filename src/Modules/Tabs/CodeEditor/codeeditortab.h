#ifndef CODEEDITORTAB_H
#define CODEEDITORTAB_H

#include "libs/CodeEditor/include/widgets/CustomCodeEditor.h"
#include "core/modules/TabBase.h"
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QWidget>

class CodeEditorTab : public TabBase
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
    QLineEdit* m_replaceEdit = nullptr;
    QLabel* m_searchStatusLabel = nullptr;
    QPushButton* m_searchPrevButton = nullptr;
    QPushButton* m_searchNextButton = nullptr;
    QPushButton* m_replaceButton = nullptr;
    QPushButton* m_replaceAllButton = nullptr;
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
    QShortcut* m_replaceShortcut = nullptr;
    bool m_replaceMode = false;
    QString m_currentLang = "Plain Text";

    static QString detectLanguage(const QString& filePath);
    void openFindDialog();
    void openReplaceDialog();
    void findNext(bool forward = true);
    void openGoToLineDialog();
    void updateSearchUi();
    void setReplaceMode(bool enabled);
    void replaceCurrent();
    void replaceAll();
    void closeSearchBar();

public:
    explicit CodeEditorTab(QWidget *parent = nullptr);

    QIcon icon() const override { return QIcon(":/icons/code.svg"); };

    void setFileDataBuffer(FileDataBuffer* newFileDataBuffer) override;

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

    void setWordWrapSlot(bool checked) override;
    void setTabReplaceSlot(bool checked) override;
    void setTabWidthSlot(int width) override;

};

#endif // CODEEDITORTAB_H
