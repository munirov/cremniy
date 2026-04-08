#ifndef REVERSECALCULATORDIALOG_H
#define REVERSECALCULATORDIALOG_H

#include <QDialog>
#include <QMap>
#include <QString>

class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QListWidget;
class QListWidgetItem;

class ReverseCalculatorDialog final : public QDialog {
    Q_OBJECT
public:
    explicit ReverseCalculatorDialog(QWidget *parent = nullptr);

private slots:
    void onInputChanged();
    void onReturnPressed();
    void onHistoryItemClicked(QListWidgetItem *item);
    void onSwapEndian();

private:
    void copyText(const QString &text);
    void updateOutputs(qulonglong value, bool ok);
    void clearOutputs();

    struct OutputRow {
        QLabel *value = nullptr;
        QPushButton *copyBtn = nullptr;
    };

    QLineEdit *m_input = nullptr;
    QComboBox *m_width = nullptr;
    QListWidget *m_historyList = nullptr;
    QPushButton *m_clearHistoryBtn = nullptr;
    QLabel *m_status = nullptr;
    
    QMap<QString, OutputRow> m_rows;
    QPushButton *m_copyAllBtn = nullptr;
    QPushButton *m_swapBtn = nullptr;
};

#endif // REVERSECALCULATORDIALOG_H
