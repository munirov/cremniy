#ifndef ICONPROVIDER_H
#define ICONPROVIDER_H

#include <QFileIconProvider>
#include <QMimeDatabase> // Нужен для определения типов файлов

class IconProvider : public QFileIconProvider
{
public:
    IconProvider(); // Объявление конструктора
    QIcon icon(const QFileInfo &info) const override;

private:
    QMimeDatabase m_mimeDb; // База данных типов (одна на весь класс)
};

#endif // ICONPROVIDER_H
