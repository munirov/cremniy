#include "OutputPanel.h"
#include <QVBoxLayout>
#include <QScrollBar>

OutputPanel::OutputPanel(QWidget* parent) : QDockWidget("Output", parent) {
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_output = new QPlainTextEdit;
    m_output->setReadOnly(true);
    m_output->setFont(QFont("Monospace", 10));
    setWidget(m_output);
}

void OutputPanel::appendLine(const QString& line) {
    m_output->appendPlainText(line);
    m_output->verticalScrollBar()->setValue(
        m_output->verticalScrollBar()->maximum());
}

void OutputPanel::clear() { m_output->clear(); }