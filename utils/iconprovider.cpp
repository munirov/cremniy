#include "iconprovider.h"

#include <QFileIconProvider>
#include <QIcon>

QIcon IconProvider::icon(const QFileInfo &info) const {
    if (info.isDir()) {
        return QIcon(":/icons/dasm.png");
    } else if (info.suffix() == "txt") {
        return QIcon(":/icons/code.png");
    }
    return QIcon(":/icons/code.png"); // дефолтная иконка
}
