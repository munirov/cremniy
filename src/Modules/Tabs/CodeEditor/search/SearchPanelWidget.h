#ifndef SEARCHPANELWIDGET_H
#define SEARCHPANELWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QStackedWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include "SearchDefs.h"

class CustomCodeEditor;

class SearchPanelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SearchPanelWidget(CustomCodeEditor* editor, QWidget* parent = nullptr);

    void setEditor(CustomCodeEditor* editor);
    void showInFileMode();
    void showProjectMode();

    void findNext();
    void findPrevious();

signals:
    void openFileAtLine(const QString& filePath, int lineNumber, const QString& highlightText);

private slots:
    void onModeChanged(int index);
    void onInFileSearchTextChanged(const QString& text);
    void onInFileReplaceCurrent();
    void onInFileReplaceAll();
    void onInFileUpdateStatus();
    void onInFileDebounceTimeout();
    void onProjectSearch();
    void onProjectSearchTextChanged(const QString& text);
    void onProjectDebounceTimeout();
    void onProjectReplaceCurrent();
    void onProjectReplaceAll();
    void onProjectResultActivated(const SearchResult& result);

private:
    CustomCodeEditor* m_editor = nullptr;

    // Mode selector
    QComboBox* m_modeCombo;
    QStackedWidget* m_stack;

    // In File page
    QWidget* m_inFilePage;
    QLineEdit* m_inFileSearchEdit;
    QLineEdit* m_inFileReplaceEdit;
    QPushButton* m_inFilePrevButton;
    QPushButton* m_inFileNextButton;
    QPushButton* m_inFileReplaceButton;
    QPushButton* m_inFileReplaceAllButton;
    QCheckBox* m_inFileMatchCase;
    QCheckBox* m_inFileRegex;
    QCheckBox* m_inFileWholeWord;
    QLabel* m_inFileStatusLabel;
    QTimer* m_inFileDebounce;

    // Across Project page
    QWidget* m_projectPage;
    QLineEdit* m_projectSearchEdit;
    QLineEdit* m_projectReplaceEdit;
    QPushButton* m_projectSearchButton;
    QPushButton* m_projectReplaceAllButton;
    QCheckBox* m_projectMatchCase;
    QCheckBox* m_projectRegex;
    QCheckBox* m_projectWholeWord;
    QCheckBox* m_projectOpenFilesOnly;
    QLabel* m_projectStatusLabel;
    QProgressBar* m_projectProgressBar;
    QWidget* m_projectResultsContainer;
    QTimer* m_projectDebounce;

    void setupInFilePage();
    void setupProjectPage();
    void setupConnections();
    QString findProjectPath() const;
    QStringList openFilePaths() const;
};

#endif // SEARCHPANELWIDGET_H
