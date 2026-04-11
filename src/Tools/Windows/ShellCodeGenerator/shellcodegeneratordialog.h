#ifndef SHELLCODEGENERATORDIALOG_H
#define SHELLCODEGENERATORDIALOG_H

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <cstdint>
#include <vector>

class ShellcodeGeneratorDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ShellcodeGeneratorDialog(QWidget *parent = nullptr);

  private slots:
    void onAssemble();
    void onCopyOutput();
    void onClear();

  private:
    struct DisasmEntry {
        int offset;
        int size;
        QString mnemonic;
        DisasmEntry(int o, int s, const QString &m) : offset(o), size(s), mnemonic(m) {}
    };

    QList<DisasmEntry> disassemble(const QByteArray &raw, const QString &bits) const;

    static QString formatAnnotated(const QByteArray &raw, const QList<DisasmEntry> &entries);

    QString generateC(const QByteArray &raw, const QString &bits) const;
    QString generateCpp(const QByteArray &raw, const QString &bits) const;
    QString generateRaw(const QByteArray &raw) const;

    void setStatus(const QString &msg, bool error = false);

    static QString findTool(const QString &name);
    bool checkDependencies();

    QTextEdit *m_asmInput = nullptr;
    QTextEdit *m_shellcodeOutput = nullptr;
    QComboBox *m_shellcodeStyle = nullptr;
    QComboBox *m_archCombo = nullptr;
    QPushButton *m_copyBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_byteCountLabel = nullptr;
};

#endif // SHELLCODEGENERATORDIALOG_H
