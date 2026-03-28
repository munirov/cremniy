#ifndef ICONPROVIDER_H
#define ICONPROVIDER_H

#include <QFileIconProvider>

class IconProvider : public QFileIconProvider
{
public:
    QIcon icon(const QFileInfo &info) const override;
};

#endif // ICONPROVIDER_H
