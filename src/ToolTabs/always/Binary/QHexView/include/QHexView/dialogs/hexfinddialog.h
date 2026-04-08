#pragma once

#include <QDialog>
#include <QHexView/model/qhexutils.h>

class QRegularExpressionValidator;
class QDoubleValidator;
class QIntValidator;
class QHexView;

class HexFindDialog: public QDialog {
    Q_OBJECT

public:
    enum class Type { Find, Replace };

public:
    explicit HexFindDialog(HexFindDialog::Type type = Type::Find,
                           QHexView* parent = nullptr);
    QHexView* hexView() const;

private Q_SLOTS:
    void updateFindOptions(int);
    void validateActions();
    void replace();
    void find();

private:
    bool prepareOptions(QString& q, QHexFindMode& mode, QHexFindDirection& fd);
    bool validateIntRange(uint v) const;
    void checkResult(const QString& q, qint64 offset, QHexFindDirection fd);
    void prepareTextMode(QLayout* l);
    void prepareHexMode(QLayout* l);
    void prepareIntMode(QLayout* l);
    void prepareFloatMode(QLayout* l);

private:
    QRegularExpressionValidator *m_hexvalidator, *m_hexpvalidator;
    QDoubleValidator* m_dblvalidator;
    QIntValidator* m_intvalidator;
    int m_oldidxbits{-1}, m_oldidxendian{-1};
    unsigned int m_findoptions{0};
    qint64 m_startoffset{-1};
    Type m_type;

private:
    static const QString BUTTONBOX;
    static const QString CBFINDMODE;
    static const QString LEFIND;
    static const QString LEREPLACE;
    static const QString HLAYOUT;
    static const QString GBOPTIONS;
    static const QString RBALL;
    static const QString RBFORWARD;
    static const QString RBBACKWARD;
};
