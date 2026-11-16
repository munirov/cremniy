#ifndef LOGVIEW_H
#define LOGVIEW_H

#include <QObject>
#include <QPlainTextEdit>
#include <QWidget>

class logView : public QPlainTextEdit {
    Q_OBJECT

    public:
        logView();

    public slots:
        void onErrorReceived(const QString &log);
        void onOutputReceived(const QString &log);

};

#endif// LOGVIEW_H
