#include "widgets/BlameTooltip.h"
#include <QPainter>
#include <QPainterPath>
#include <QLocale>
#include <QDateTime>
#include <QFontMetrics>
#include <QScreen>
#include <QApplication>

BlameTooltip::BlameTooltip(QWidget* parent)
    : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(320, 140); /* Default size, will be adjusted if needed */
}

void BlameTooltip::setBlameInfo(const EditorBlameLineInfo& info)
{
    m_info = info;

    /* Calculate required height based on commit summary */
    QFont font = this->font();
    font.setPointSize(10);
    QFontMetrics fm(font);

    int textWidth = width() - 24;
    QRect summaryRect = fm.boundingRect(QRect(0, 0, textWidth, 1000),
                                      Qt::TextWordWrap, m_info.commitSummary);

    int newHeight = 100 + summaryRect.height();
    setFixedHeight(qBound(120, newHeight, 300));

    update();
}

void BlameTooltip::showAt(const QPoint& globalPos)
{
    /* Ensure it stays on screen */
    QScreen *screen = QApplication::screenAt(globalPos);
    QRect screenRect = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    int x = globalPos.x();
    int y = globalPos.y() - height() - 10;

    if (x + width() > screenRect.right()) x = screenRect.right() - width();
    if (y < screenRect.top()) y = globalPos.y() + 20;
    if (x < screenRect.left()) x = screenRect.left();

    move(x, y);
    show();
}

void BlameTooltip::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    /* Background: Use palette for consistency with project style */
    QColor bgColor = palette().window().color();
    if (bgColor.lightness() < 128) {
        bgColor = QColor(43, 42, 51, 245);
    } else {
        bgColor = QColor(245, 245, 245, 245);
    }

    QPainterPath path;
    path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4); /* Smaller radius for project style */

    painter.fillPath(path, bgColor);

    /* Border matching project style */
    painter.setPen(palette().mid().color());
    painter.drawPath(path);

    int x = 12;
    int y = 22;

    /* Author Name */
    QFont authorFont = painter.font();
    authorFont.setBold(true);
    authorFont.setPointSize(10);
    painter.setFont(authorFont);
    painter.setPen(palette().text().color());
    painter.drawText(x, y, m_info.authorName);

    y += 18;

    /* Email */
    QFont emailFont = painter.font();
    emailFont.setBold(false);
    emailFont.setPointSize(8);
    painter.setFont(emailFont);
    painter.setPen(palette().placeholderText().color());
    painter.drawText(x, y, tr("<%1>").arg(m_info.authorEmail));

    y += 24;

    /* Commit Summary (Word Wrapped) */
    QFont summaryFont = painter.font();
    summaryFont.setPointSize(9);
    painter.setFont(summaryFont);
    painter.setPen(palette().text().color());

    QRect summaryRect(x, y, width() - 24, height() - y - 30);
    painter.drawText(summaryRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_info.commitSummary);

    /* Footer: Hash and Date */
    QFont footerFont = painter.font();
    footerFont.setFamilies({"JetBrains Mono", "monospace"});
    footerFont.setPointSize(8);
    painter.setFont(footerFont);
    painter.setPen(palette().placeholderText().color());

    QString footerText = tr("%1 • %2")
        .arg(m_info.fullOid.left(10))
        .arg(QLocale::system().toString(m_info.commitDate, QLocale::ShortFormat));

    painter.drawText(rect().adjusted(12, -8, -12, -8), Qt::AlignBottom | Qt::AlignLeft, footerText);
}

void BlameTooltip::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
}
