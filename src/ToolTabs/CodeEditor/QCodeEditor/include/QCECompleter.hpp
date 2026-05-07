#pragma once

#include <QCompleter> /* Required for inheritance */

/**
 * @brief Class, that describes completer with
 * glsl specific types and functions.
 */
class QCECompleter : public QCompleter
{
    Q_OBJECT

public:

    /**
     * @brief Constructor.
     * @param parent Pointer to parent QObject.
     */
    explicit QCECompleter(QString langFile, QObject* parent=nullptr);
};


