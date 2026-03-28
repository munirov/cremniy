#include <QTabBar>
#include <QStylePainter>
#include <QStyleOptionTab>

class VerticalTabBar : public QTabBar
{
public:
    QSize tabSizeHint(int index) const override
    {
        QSize s = QTabBar::tabSizeHint(index);
        s.transpose();
        return s;
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStylePainter painter(this);
        QStyleOptionTab opt;

        for (int i = 0; i < count(); i++)
        {
            initStyleOption(&opt, i);
            painter.drawControl(QStyle::CE_TabBarTabShape, opt);

            painter.save();

            QSize s = opt.rect.size();
            s.transpose();
            QRect r(QPoint(), s);
            r.moveCenter(opt.rect.center());

            opt.rect = r;

            painter.translate(opt.rect.center());
            painter.rotate(90);
            painter.translate(-opt.rect.center());

            painter.drawControl(QStyle::CE_TabBarTabLabel, opt);

            painter.restore();
        }
    }
};