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
    /**
     * @brief Виджет редактора кода
    */
    CustomCodeEditor* m_codeEditorWidget;

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
    bool m_largeFileMode = false;
    bool m_gitBlameEnabled = false;
    bool m_updatingSelection = false;
    QShortcut* m_goToLineShortcut = nullptr;
    QString m_currentLang = "Plain Text";

    static QString detectLanguage(const QString& filePath);
    void openGoToLineDialog();

public:
    explicit CodeEditorTab(QWidget *parent = nullptr);

    QIcon icon() const override { return QIcon(":/icons/code.svg"); };
    bool gitBlameEnabled() const override { return m_gitBlameEnabled; }
    QString selectedSearchText() const;
    bool revealSearchMatch(int oneBasedLine, int zeroBasedColumn, int length);

    /**
     * @brief Текущая строка курсора редактора (нумерация с единицы)
     *
     * Используется для восстановления позиции чтения при перезагрузке файла
     */
    qint64 currentCursorLine() const {
        return m_codeEditorWidget ? m_codeEditorWidget->cursorLine() : -1;
    }

    /**
     * @brief Перейти на указанную строку редактора (нумерация с единицы)
     */
    void goToCursorLine(qint64 oneBasedLine) {
        if (m_codeEditorWidget)
            m_codeEditorWidget->goToLine(oneBasedLine);
    }

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
    void setGitBlameSlot(bool checked) override;
    void setGitBlameData(const QString &filePath,
                         const QVector<TabGitBlameLineInfo> &lines) override;
    void setGitBlameError(const QString &filePath, const QString &error) override;
    void refreshGitBlame() override;

private slots:
    void requestBlameUpdate();
};

#endif // CODEEDITORTAB_H
