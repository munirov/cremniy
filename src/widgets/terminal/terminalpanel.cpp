#include "terminalpanel.h"

#include "terminalwidget.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QToolButton *createButton(const QString &iconName,
                          const QString &text,
                          const QString &toolTip,
                          QWidget *parent)
{
    auto *button = new QToolButton(parent);
    const QIcon icon(QStringLiteral(":/icons/phoicons/icons/%1.svg").arg(iconName));
    if (icon.isNull())
        button->setText(text);
    else
        button->setIcon(icon);
    button->setAutoRaise(true);
    button->setToolTip(toolTip);
    button->setAccessibleName(toolTip);
    button->setFixedSize(28, 28);
    return button;
}

} // namespace

TerminalPanel::TerminalPanel(const QString &workingDirectory, QWidget *parent)
    : QWidget(parent)
    , m_workingDirectory(workingDirectory)
    , m_projectName(QFileInfo(workingDirectory).fileName())
{
    if (m_projectName.isEmpty())
        m_projectName = QCoreApplication::applicationName();

    setObjectName(QStringLiteral("terminalPanel"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setMovable(true);
    m_tabs->setTabsClosable(true);
    m_tabs->tabBar()->setElideMode(Qt::ElideRight);
    m_tabs->tabBar()->setExpanding(false);
    layout->addWidget(m_tabs);

    auto *actions = new QWidget(m_tabs);
    auto *actionsLayout = new QHBoxLayout(actions);
    actionsLayout->setContentsMargins(0, 0, 4, 0);
    actionsLayout->setSpacing(0);
    QToolButton *newButton = createButton(QStringLiteral("plus"),
                                          QStringLiteral("+"),
                                          tr("New Terminal"),
                                          actions);
    m_closeButton = createButton(QStringLiteral("trash"),
                                 QStringLiteral("×"),
                                 tr("Kill Terminal"),
                                 actions);
    actionsLayout->addWidget(newButton);
    actionsLayout->addWidget(m_closeButton);
    m_tabs->setCornerWidget(actions, Qt::TopRightCorner);

    connect(newButton, &QToolButton::clicked, this, &TerminalPanel::newTerminal);
    connect(m_closeButton, &QToolButton::clicked, this, &TerminalPanel::closeActiveTerminal);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &TerminalPanel::closeTerminalTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        updateActions();
        focusActiveTerminal();
    });

    newTerminal();
}

void TerminalPanel::focusActiveTerminal()
{
    if (TerminalWidget *terminal = currentTerminal())
        terminal->setFocus(Qt::ShortcutFocusReason);
}

void TerminalPanel::newTerminal()
{
    const int terminalNumber = m_nextTerminalNumber++;
    auto *terminal = new TerminalWidget(m_tabs, m_workingDirectory);
    const QString title = tr("[%1] %2").arg(terminalNumber).arg(m_projectName);
    const int index = m_tabs->addTab(terminal, title);
    m_tabs->setTabToolTip(index, m_workingDirectory);
    m_tabs->setCurrentIndex(index);

    connect(terminal, &TerminalWidget::newTerminalRequested,
            this, &TerminalPanel::newTerminal);
    connect(terminal, &TerminalWidget::closeRequested, this, [this, terminal] {
        closeTerminalTab(m_tabs->indexOf(terminal));
    });
    updateActions();
    terminal->setFocus(Qt::ShortcutFocusReason);
}

void TerminalPanel::closeActiveTerminal()
{
    closeTerminalTab(m_tabs->currentIndex());
}

TerminalWidget *TerminalPanel::currentTerminal() const
{
    return qobject_cast<TerminalWidget *>(m_tabs->currentWidget());
}

void TerminalPanel::closeTerminalTab(int index)
{
    if (index < 0 || index >= m_tabs->count())
        return;
    QWidget *terminal = m_tabs->widget(index);
    m_tabs->removeTab(index);
    terminal->deleteLater();

    if (m_tabs->count() == 0) {
        emit closeRequested();
        return;
    }

    updateActions();
    focusActiveTerminal();
}

void TerminalPanel::updateActions()
{
    m_closeButton->setEnabled(currentTerminal() != nullptr);
}
