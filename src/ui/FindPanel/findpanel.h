#pragma once

#include "core/FindEngine/findtypes.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class FindPanel final : public QWidget {
    Q_OBJECT

public:
    explicit FindPanel(const QString& projectPath, QWidget* parent = nullptr);

    void open(SearchScope scope, bool replaceMode, const QString& initialQuery = {});
    void nextResult();
    void previousResult();
    void refresh();
    void setReport(const SearchReport& report);
    void setSearching(bool searching);

signals:
    void closeRequested();
    void statusMessage(const QString& message);
    void searchInvalidated();
    void searchRequested(const SearchOptions& options);
    void resultActivated(const SearchMatch& match);
    void replaceRequested(const SearchMatch& match, const SearchOptions& options);
    void replaceAllRequested(const SearchReport& report, const SearchOptions& options);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void scheduleSearch();
    void activateCurrentResult();
    void replaceCurrentResult();
    void replaceAllResults();

private:
    SearchOptions currentOptions() const;
    void buildUi();
    void setReplaceMode(bool enabled);
    void showReport(const SearchReport& report);
    void showEmptyState(const QString& title, const QString& detail = {});
    void selectAdjacentResult(bool forward);
    int selectedMatchIndex() const;
    QString displayPath(const QString& filePath) const;

    QString m_projectPath;
    QLineEdit* m_searchEdit = nullptr;
    QLineEdit* m_replaceEdit = nullptr;
    QComboBox* m_scopeCombo = nullptr;
    QToolButton* m_replaceToggle = nullptr;
    QToolButton* m_matchCaseButton = nullptr;
    QToolButton* m_wholeWordButton = nullptr;
    QToolButton* m_regexButton = nullptr;
    QLineEdit* m_includeEdit = nullptr;
    QLineEdit* m_excludeEdit = nullptr;
    QWidget* m_filterRow = nullptr;
    QWidget* m_replaceRow = nullptr;
    QPushButton* m_replaceButton = nullptr;
    QPushButton* m_replaceAllButton = nullptr;
    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_emptyTitle = nullptr;
    QLabel* m_emptyDetail = nullptr;
    QTreeWidget* m_resultsTree = nullptr;
    QStackedWidget* m_resultsStack = nullptr;
    QTimer* m_searchTimer = nullptr;
    SearchReport m_report;
    bool m_replaceMode = false;
};
