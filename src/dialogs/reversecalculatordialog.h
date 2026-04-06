// ===== reversecalculatordialog.h =====
// SPDX-License-Identifier: MIT
#ifndef REVERSECALCULATORDIALOG_H
#define REVERSECALCULATORDIALOG_H

#include <QDialog>

class QComboBox;
class QLineEdit;
class QLabel;
class QCheckBox;
class QPushButton;
class QFormLayout;
class QListWidget;
class QListWidgetItem;

class ReverseCalculatorDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit ReverseCalculatorDialog(QWidget* parent = nullptr);

private slots:
    void onInputChanged();
    void onReturnPressed();
    void onHistoryItemClicked(QListWidgetItem* item);
    void onSwapEndian();
    void onCopyHex();
    void onCopyUINT();
    void onCopyINT();
    void onCopyBin();

private:
    void updateOutputs(qulonglong value, bool ok);
    void updateSignedRowVisibility();

    static bool parseValue(const QString& text, qulonglong* outValue);
    static bool parseExpression(const QString& text, qulonglong* outValue, QString* errorOut, int* lhsBase = nullptr);
    static qulonglong maskToWidth(qulonglong v, int bits);
    static qlonglong  toSigned(qulonglong v, int bits);
    static qulonglong swapEndian(qulonglong v, int bits);

    QLineEdit* m_input = nullptr;
    QComboBox* m_width = nullptr;
    QCheckBox* m_showSigned = nullptr;

    QFormLayout* m_form = nullptr;

    QLabel* m_status = nullptr;
    QLabel* m_hex = nullptr;
    QLabel* m_decU = nullptr;
    QLabel* m_decS = nullptr;
    QLabel* m_bin = nullptr;

    QPushButton* m_swapBtn = nullptr;
    QPushButton* m_copyHex = nullptr;
    QPushButton* m_copyDecU = nullptr;
    QPushButton* m_copyDecS = nullptr;
    QPushButton* m_copyBin = nullptr;

    QListWidget* m_historyList = nullptr;
    QPushButton* m_clearHistoryBtn = nullptr;
};

#endif // REVERSECALCULATORDIALOG_H