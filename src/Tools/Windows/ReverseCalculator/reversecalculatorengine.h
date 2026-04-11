#ifndef REVERSECALCULATORENGINE_H
#define REVERSECALCULATORENGINE_H

#include <QString>
#include <optional>
#include <QtGlobal>

struct ParseResult {
    qulonglong value = 0;
    QString error;
    bool ok = false;
    int base = 10;
};

class ReverseCalculatorEngine {
public:
    static ReverseCalculatorEngine& instance();

    ParseResult parseExpression(const QString &expression, int bits);

    static QString formatHex(qulonglong v, int bits);
    static QString formatBin(qulonglong v, int bits);
    static QString formatOct(qulonglong v, int bits);
    static QString formatBytes(qulonglong v, int bits);
    static QString formatSigned(qulonglong v, int bits);
    static QString formatUnsigned(qulonglong v, int bits);

    static qulonglong maskToWidth(qulonglong v, int bits);
    static qlonglong toSigned(qulonglong v, int bits);
    static qulonglong swapEndian(qulonglong v, int bits);
    static int detectBase(const QString &token);

private:
    ReverseCalculatorEngine() = default;
    
    struct Token {
        enum Type { Number, Operator, OpenParen, CloseParen } type;
        QString value;
        int precedence = 0;
        bool rightAssociative = false;
        bool unary = false;
    };

    QList<Token> tokenize(const QString &expression);
    std::optional<qulonglong> parseValue(const QString &text);
};

#endif // REVERSECALCULATORENGINE_H
