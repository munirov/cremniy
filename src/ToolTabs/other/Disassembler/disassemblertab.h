#ifndef DISASSEMBLERTAB_H
#define DISASSEMBLERTAB_H

#include "core/ToolTab.h"
#include "disassemblerworker.h"
#include <QIcon>
#include <QWidget>
#include <QThread>
#include <QHash>

#include <QGuiApplication>
#include <QClipboard>

class QPlainTextEdit;
class QLabel;
class QProgressBar;
class QPushButton;
class QComboBox;
class QLineEdit;
class QStackedWidget;
class QSplitter;
class QTimer;
class DisasmTextHighlighter;
class QListWidget;


class DisassemblerTab : public ToolTab
{
    Q_OBJECT

public:
    explicit DisassemblerTab(FileDataBuffer* buffer, QWidget *parent = nullptr);
    ~DisassemblerTab();

    QString toolName() const override { return "Disassembler"; };
    QIcon toolIcon() const override { return QIcon(":/icons/dasm.png"); };

    void saveToFile(QString path) {}
    void setTabData(QByteArray &data);

    struct LineInfo {
        int sectionIndex = -1; // index in m_sections
        QString address;
        QString bytes;
        QString mnemonic;
        QString operands;
        qint64 fileOffset = -1;
        qint64 size = 0;

        // Cached lowercase for fast search
        QString addrL;
        QString bytesL;
        QString mnemL;
        QString opsL;
    };

signals:
    void requestDisassembly(const QString &filePath, const QString &arch);

public slots:

    // From Parrent Class: ToolTab
    void setFile(QString filepath) override;
    void setTabData() override;
    void saveTabData() override;

protected slots:
    // Обработчик изменения выделения из буфера
    void onSelectionChanged(qint64 pos, qint64 length) override;
    void onDataChanged() override;

private slots:

    void onSectionFound(const DisasmSection &section);
    void onFunctionsFound(const QVector<DisasmFunction> &funcs);
    void onStringsFound(const QVector<DisasmString> &strings);
    void onWorkerFinished();
    void onWorkerError(const QString &msg);
    void onProgressUpdated(int percent);
    void onLogLine(const QString &line);
    void onSearchTextChanged(const QString &text);
    void onSectionComboChanged(int index);
    void startDisassembly();
    void cancelDisassembly();
    void onGlobalActionTriggered(const QString &actionName);

private:
    enum class StatusMode {
        Idle,
        Loading,
        Ready,
        Empty,
        Error,
    };

    bool eventFilter(QObject *watched, QEvent *event) override;
    void showInstructionHelpAt(const QPoint &pos, bool forceByCursor = false);

    void setupUi();
    void setRunningState(bool running);
    void showPlaceholder(const QString &msg);
    void updateBackendUiHint();
    void populateSectionCombo();
    void applyFilter();
    void appendLog(const QString &line);
    QString formatLine(const LineInfo &li) const;
    void rebuildFunctionsFromLines();          // objdump labels
    void setFunctionsList(const QVector<DisasmFunction> &funcs);
    void jumpToAddress(const QString &addr);
    const LineInfo *currentLineInfo(int *visibleLine = nullptr) const;
    int currentInstructionOrdinal(int visibleLine) const;
    void updateStatusState();
    QString autoCommentForLine(const LineInfo &li) const;
    bool tryResolveStringRefAddr(const LineInfo &li, quint64 *outAddr) const;

    QThread            *m_thread  = nullptr;
    DisassemblerWorker *m_worker  = nullptr;
    bool                m_running = false;
    bool                m_updatingSelection = false; // Флаг для предотвращения рекурсии

    QVector<DisasmSection> m_sections;
    QVector<DisasmFunction> m_functions;
    QVector<DisasmString> m_strings;
    QHash<quint64, QString> m_stringByAddr; // for fast auto-comments

    QVector<LineInfo> m_lines;
    QVector<int> m_visibleLineMap; // visible line idx -> m_lines idx
    QTimer *m_searchDebounce = nullptr;

    // UI
    QWidget        *m_toolbar        = nullptr;
    QComboBox      *m_sectionCombo   = nullptr;
    QLineEdit      *m_searchEdit     = nullptr;
    QPushButton    *m_runBtn         = nullptr;
    QPushButton    *m_cancelBtn      = nullptr;
    QPushButton    *m_logToggleBtn   = nullptr;
    QProgressBar   *m_progressBar    = nullptr;
    QLabel         *m_statusLabel    = nullptr;
    QSplitter      *m_splitter       = nullptr;
    QStackedWidget *m_stack          = nullptr;
    QSplitter      *m_topSplitter    = nullptr; // functions | listing
    QWidget        *m_funcPanel      = nullptr;
    QLineEdit      *m_funcFilterEdit = nullptr;
    QListWidget    *m_funcList       = nullptr;
    QPlainTextEdit *m_disasmView     = nullptr;
    DisasmTextHighlighter *m_disasmHighlighter = nullptr;
    QLabel         *m_placeholderLbl = nullptr;
    QWidget        *m_logPanel       = nullptr;
    QPlainTextEdit *m_logView        = nullptr;

    int m_currentSectionIndex = -1;
    QTimer *m_refreshDebounce = nullptr;
    StatusMode m_statusMode = StatusMode::Idle;
    QString m_statusDetail;
};

#endif // DISASSEMBLERTAB_H
