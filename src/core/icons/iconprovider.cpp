#include "iconprovider.h"
#include <QIcon>
#include <QPixmap>
#include <QPainter>

IconProvider::IconProvider() : QFileIconProvider() 
{
}

// Универсальная функция для покраски в любой цвет
QIcon paintIcon(const QIcon &icon, QColor color) {
    if (icon.isNull()) return icon;

    // Берем стандартный размер для дерева
    QPixmap pixmap = icon.pixmap(32, 32);
    if (pixmap.isNull()) return icon;

    QPainter painter(&pixmap);
    // SourceIn заменяет всё непрозрачное на выбранный цвет
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    painter.end();

    return QIcon(pixmap);
}

QIcon IconProvider::icon(const QFileInfo &info) const {
    QString iconName;
    bool isFolder = info.isDir();

    if (isFolder) {
        iconName = "folder";
    } else {
        QString ext = info.suffix().toLower();
        QString name = info.fileName();
        if (ext == "c") iconName = "file-c";
        else if (ext == "cpp" || ext == "cxx") iconName = "file-cpp";
        else if (ext == "h" || ext == "hpp") iconName = "file-code";
        else if (ext == "asm" || ext == "s") iconName = "cpu";
        else if (ext == "ld") iconName = "tree-structure";
        else if (ext == "o" || ext == "obj") iconName = "file-dashed";
        else if (ext == "bin" || ext == "elf") iconName = "terminal-window";
        else if (ext == "iso" || ext == "img") iconName = "disc";
        else if (ext == "md") iconName = "file-md";
        else if (ext == "json") iconName = "brackets-curly";
        else if (name == "Makefile" || ext == "mk") iconName = "wrench";
        else if (name == "CMakeLists.txt" || ext == "cmake") iconName = "gear";
        else if (ext == "txt") iconName = "file-text";
        else iconName = "file";
    }

    // 1. Пытаемся достать из темы (как положено)
    QIcon ic = QIcon::fromTheme(iconName);
    
    // 2. ЕСЛИ НЕ НАШЛОСЬ (ic.isNull() или системная иконка вместо нашей)
    // Пробуем загрузить напрямую из файла в ресурсах
    if (ic.isNull() || ic.availableSizes().isEmpty()) {
        QString directPath = QString(":/icons/phoicons/icons/%1.svg").arg(iconName);
        if (QFile::exists(directPath)) {
            ic = QIcon(directPath);
        }
    }

    // Если папка всё еще пустая, пробуем simple
    if (isFolder && ic.isNull()) {
        ic = QIcon(":/icons/phoicons/icons/folder-simple.svg");
    }

    // --- ФИНАЛЬНАЯ ПОКРАСКА ---
    if (!ic.isNull()) {
        return paintIcon(ic, Qt::white);
    }

    // Если совсем ничего не помогло — дефолт ОС
    return QFileIconProvider::icon(info);
}
