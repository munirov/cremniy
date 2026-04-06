#include "terminalwidget.h"
#include <QProcessEnvironment>
#include <KodoTerm/KodoTermConfig.hpp>
#include <QTimer>
#include <QApplication>
#include <QResizeEvent>
#include <QDebug>

TerminalWidget::TerminalWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_terminal = new KodoTerm(this); 
    m_terminal->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_terminal);
    m_terminal->setMinimumWidth(600);
    KodoTermConfig config;
    QFont monoFont("Consolas", 10);
    monoFont.setFixedPitch(true);
    monoFont.setKerning(false); 
    monoFont.setStyleStrategy(QFont::NoFontMerging); // Чтобы не подтягивало символы из других шрифтов, ломая ширину
    config.font = monoFont;
    m_terminal->setConfig(config);

    applyTheme(true);

    // Устанавливаем фильтр событий только на сам виджет терминала
    m_terminal->installEventFilter(this);
}

void TerminalWidget::applyTheme(bool isDark) {
    KodoTermConfig config = m_terminal->getConfig();
    
    // Настройка шрифта "на максималках"
    QFont monoFont;
    monoFont.setFamily("Consolas");
    monoFont.setStyleHint(QFont::Monospace); // Это заставит Qt искать любой моно-шрифт, если Consolas нет
    monoFont.setFixedPitch(true);
    monoFont.setPointSize(10);
    monoFont.setKerning(false);
    
    config.font = monoFont;
    
    QString themePath = isDark 
        ? ":/KodoTermThemes/konsole/Breeze.colorscheme" 
        : ":/KodoTermThemes/konsole/BlackOnWhite.colorscheme";

    config.theme = TerminalTheme::loadTheme(themePath);
    m_terminal->setConfig(config);
}
bool TerminalWidget::eventFilter(QObject *obj, QEvent *event) {
    // Ловим Resize, но ТОЛЬКО если терминал еще не запущен
    if (!m_isStarted && obj == m_terminal && event->type() == QEvent::Resize) {
        if (m_terminal->width() > 200) {
            // Вызываем старт через таймер 0, чтобы выйти из текущего обработчика событий 
            // и избежать любой возможности рекурсии
            QTimer::singleShot(0, this, &TerminalWidget::startShell);
        }
    }
    return QWidget::eventFilter(obj, event);
}

void TerminalWidget::startShell() {
    if (m_isStarted) return;

    QString shell;
#ifdef Q_OS_WIN
    shell = "powershell.exe";
#else
    shell = qEnvironmentVariable("SHELL", "/bin/bash");
#endif

    m_terminal->setProgram(shell);
    m_terminal->start(); 
    m_isStarted = true;
    
    qDebug() << "Terminal started with width:" << m_terminal->width();
}

void TerminalWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    // Когда терминал снова виден, принудительно заставляем его перерисоваться
    if (m_terminal) {
        m_terminal->update(); 
        // Если метод damageAll публичный (или через мета-объект):
        QMetaObject::invokeMethod(m_terminal, "damageAll"); 
    }
}