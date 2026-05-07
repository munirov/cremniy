#include "reversecalculatorengine.h"
#include <QRegularExpression>
#include <QtEndian>
#include <QStringBuilder>
#include <QStack>
#include <cmath>

ReverseCalculatorEngine &ReverseCalculatorEngine::instance() {
    static ReverseCalculatorEngine inst;
    return inst;
}

qulonglong ReverseCalculatorEngine::maskToWidth(qulonglong v, int bits) {
    if (bits >= 64) return v;
    return v & ((1ULL << bits) - 1ULL);
}

qlonglong ReverseCalculatorEngine::toSigned(qulonglong v, int bits) {
    if (bits >= 64) return static_cast<qlonglong>(v);
    const qulonglong signBit = 1ULL << (bits - 1);
    const qulonglong mask = (1ULL << bits) - 1ULL;
    v &= mask;
    if (!(v & signBit)) return static_cast<qlonglong>(v);
    return -static_cast<qlonglong>((~v + 1ULL) & mask);
}

qulonglong ReverseCalculatorEngine::swapEndian(qulonglong v, int bits) {
    if (bits == 8) return v;
    if (bits == 16) return qbswap(static_cast<quint16>(v));
    if (bits == 32) return qbswap(static_cast<quint32>(v));
    if (bits == 64) return qbswap(static_cast<quint64>(v));
    return v;
}

int ReverseCalculatorEngine::detectBase(const QString &token) {
    const QString t = token.trimmed();
    if (t.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) return 16;
    if (t.startsWith(QLatin1String("0b"), Qt::CaseInsensitive)) return 2;
    if (t.startsWith(QLatin1String("0o"), Qt::CaseInsensitive)) return 8;
    return 10;
}

QString ReverseCalculatorEngine::formatHex(qulonglong v, int bits) {
    const int nyb = qMax(1, bits / 4);
    return QStringLiteral("0x") % QString::number(v, 16).toUpper().rightJustified(nyb, '0');
}

QString ReverseCalculatorEngine::formatBin(qulonglong v, int bits) {
    QString s;
    s.reserve(bits + bits / 8);
    for (int i = bits - 1; i >= 0; --i) {
        s += ((v >> i) & 1ULL) ? u'1' : u'0';
        if (i % 8 == 0 && i != 0) s += u' ';
    }
    return s;
}

QString ReverseCalculatorEngine::formatOct(qulonglong v, int bits) {
    const int digits = qMax(1, static_cast<int>(std::ceil(bits / 3.0)));
    return QStringLiteral("0o") % QString::number(v, 8).rightJustified(digits, '0');
}

QString ReverseCalculatorEngine::formatBytes(qulonglong v, int bits) {
    const int nBytes = qMax(1, bits / 8);
    QStringList parts;
    parts.reserve(nBytes);
    for (int i = nBytes - 1; i >= 0; --i) {
        parts << QString::number((v >> (i * 8)) & 0xFF, 16).toUpper().rightJustified(2, '0');
    }
    return parts.join(' ');
}

QString ReverseCalculatorEngine::formatSigned(qulonglong v, int bits) {
    return QString::number(toSigned(v, bits));
}

QString ReverseCalculatorEngine::formatUnsigned(qulonglong v, int bits) {
    return QString::number(maskToWidth(v, bits));
}

ParseResult ReverseCalculatorEngine::parseExpression(const QString &expression, int bits) {
    ParseResult result;
    if (expression.trimmed().isEmpty()) {
        result.error = "Empty expression";
        return result;
    }

    QList<Token> tokens = tokenize(expression);
    if (tokens.isEmpty()) {
        result.error = "Invalid characters";
        return result;
    }

    // detect base from first operand if possible
    for (const auto &t : tokens) {
        if (t.type == Token::Number) {
            result.base = detectBase(t.value);
            break;
        }
    }

    QList<Token> output;
    QStack<Token> operators;

    for (const auto &token : tokens) {
        if (token.type == Token::Number) {
            output.append(token);
        } else if (token.type == Token::OpenParen) {
            operators.push(token);
        } else if (token.type == Token::CloseParen) {
            bool found = false;
            while (!operators.isEmpty()) {
                if (operators.top().type == Token::OpenParen) {
                    operators.pop();
                    found = true;
                    break;
                }
                output.append(operators.pop());
            }
            if (!found) {
                result.error = "Unmatched closing parenthesis";
                return result;
            }
        } else if (token.type == Token::Operator) {
            while (!operators.isEmpty() && operators.top().type == Token::Operator) {
                const auto &top = operators.top();
                if ((!token.rightAssociative && token.precedence <= top.precedence) ||
                    (token.rightAssociative && token.precedence < top.precedence)) {
                    output.append(operators.pop());
                } else {
                    break;
                }
            }
            operators.push(token);
        }
    }

    while (!operators.isEmpty()) {
        if (operators.top().type == Token::OpenParen) {
            result.error = "Unmatched opening parenthesis";
            return result;
        }
        output.append(operators.pop());
    }

    QStack<qulonglong> stack;
    for (const auto &token : output) {
        if (token.type == Token::Number) {
            auto val = parseValue(token.value);
            if (!val) {
                result.error = "Invalid number (" + token.value + ")";
                return result;
            }
            stack.push(*val);
        } else if (token.type == Token::Operator) {
            if (token.unary) {
                if (stack.isEmpty()) {
                    result.error = "Missing operand for " + token.value;
                    return result;
                }
                qulonglong v = stack.pop();
                if (token.value == "~") stack.push(~v);
                else if (token.value == "-") stack.push(-v);
                else if (token.value == "+") stack.push(v);
            } else {
                if (stack.size() < 2) {
                    result.error = "Missing operand for " + token.value;
                    return result;
                }
                qulonglong rhs = stack.pop();
                qulonglong lhs = stack.pop();
                if (token.value == "+") stack.push(lhs + rhs);
                else if (token.value == "-") stack.push(lhs - rhs);
                else if (token.value == "*") stack.push(lhs * rhs);
                else if (token.value == "/") {
                    if (rhs == 0) { result.error = "Division by zero"; return result; }
                    stack.push(lhs / rhs);
                } else if (token.value == "%") {
                    if (rhs == 0) { result.error = "Modulo by zero"; return result; }
                    stack.push(lhs % rhs);
                } else if (token.value == "&") stack.push(lhs & rhs);
                else if (token.value == "|") stack.push(lhs | rhs);
                else if (token.value == "^") stack.push(lhs ^ rhs);
                else if (token.value == "<<") stack.push(lhs << (rhs & 63));
                else if (token.value == ">>") stack.push(lhs >> (rhs & 63));
            }
        }
    }

    if (stack.size() != 1) {
        result.error = "Incomplete expression";
        return result;
    }

    result.value = maskToWidth(stack.pop(), bits);
    result.ok = true;
    return result;
}

QList<ReverseCalculatorEngine::Token> ReverseCalculatorEngine::tokenize(const QString &expression) {
    QList<Token> tokens;
    static const QRegularExpression re(R"(\s*(0x[0-9a-fA-F]+|0b[01 ]+|0o[0-7]+|\d+|<<|>>|[+\-*/%&|^~()])\s*)");
    
    QRegularExpressionMatchIterator it = re.globalMatch(expression);
    int lastEnd = 0;
    while (it.hasNext()) {
        auto match = it.next();
        if (match.capturedStart() > lastEnd) {
             QString skipped = expression.mid(lastEnd, match.capturedStart() - lastEnd).trimmed();
             if (!skipped.isEmpty()) return {}; // invalid chars
        }
        
        QString val = match.captured(1).trimmed();
        Token t;
        t.value = val;
        
        if (val == "(") {
            t.type = Token::OpenParen;
        } else if (val == ")") {
            t.type = Token::CloseParen;
        } else if (QString("+-*/%&|^~<>").contains(val[0])) {
            t.type = Token::Operator;
            if (val == "~") {
                t.precedence = 7;
                t.rightAssociative = true;
                t.unary = true;
            } else if (val == "*" || val == "/" || val == "%") {
                t.precedence = 6;
            } else if (val == "+" || val == "-") {
                // check unary
                if (tokens.isEmpty() || tokens.last().type == Token::OpenParen || tokens.last().type == Token::Operator) {
                    t.precedence = 7;
                    t.rightAssociative = true;
                    t.unary = true;
                } else {
                    t.precedence = 5;
                }
            } else if (val == "<<" || val == ">>") {
                t.precedence = 4;
            } else if (val == "&") {
                t.precedence = 3;
            } else if (val == "^") {
                t.precedence = 2;
            } else if (val == "|") {
                t.precedence = 1;
            }
        } else {
            t.type = Token::Number;
        }
        tokens.append(t);
        lastEnd = match.capturedEnd();
    }
    
    if (lastEnd < expression.size()) {
         if (!expression.mid(lastEnd).trimmed().isEmpty()) return {};
    }
    
    return tokens;
}

std::optional<qulonglong> ReverseCalculatorEngine::parseValue(const QString &text) {
    QString t = text.trimmed();
    if (t.isEmpty()) return std::nullopt;

    bool ok = false;
    qulonglong v = 0;

    if (t.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
        v = t.mid(2).toULongLong(&ok, 16);
    } else if (t.startsWith(QLatin1String("0b"), Qt::CaseInsensitive)) {
        QString digits = t.mid(2).remove(' ');
        v = digits.toULongLong(&ok, 2);
    } else if (t.startsWith(QLatin1String("0o"), Qt::CaseInsensitive)) {
        v = t.mid(2).toULongLong(&ok, 8);
    } else {
        v = t.toULongLong(&ok, 10);
        // handle negative decimal by casting
        if (!ok) {
            qlonglong sv = t.toLongLong(&ok, 10);
            if (ok) v = static_cast<qulonglong>(sv);
        }
    }

    if (!ok) return std::nullopt;
    return v;
}
