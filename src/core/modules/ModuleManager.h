#ifndef MODULEMANAGER_H
#define MODULEMANAGER_H

#include <functional>
#include <QList>
#include <QString>


template<typename T>
struct ModuleDescription {
    std::function<T*()> creator;
    std::function<QString()> name;
    int position = 0;
};

template<typename T>
using ModuleStorage = QHash<QString, QVector<ModuleDescription<T>>>;

class ModuleManager {

public:
    static ModuleManager& instance();

    template<typename T>
    void registerModule(std::function<QString()> name, const QString &group, std::function<T*()> creator, int position = 0) const {
        ModuleDescription<T> desc{
            std::move(creator),
            std::move(name),
            position
        };
        getStorage<T>()[group.trimmed()].push_back(std::move(desc));
    }

    template <typename T>
    T* create(const QString& group, const int& index) {
        const auto& storage = getStorageConst<T>();
        auto it = storage.find(group.trimmed());
        if (it == storage.end()) return nullptr;

        const auto& vec = it.value();
        if (index < 0 || index >= vec.size()) return nullptr;

        return vec[index].creator();
    }

    template<typename T>
    const QVector<ModuleDescription<T>> & getByGroup(const QString &group) const { return getByGroup(group, getStorageConst<T>()); }

    template<typename T>
    QList<QString> getGroups() const { return getSortedKeys<T>(getStorageConst<T>()); }

private:
    template<typename T>
    static ModuleStorage<T>& getStorage() {
        static ModuleStorage<T> storage;
        return storage;
    }

    template<typename T>
    static const ModuleStorage<T>& getStorageConst() {
        return getStorage<T>();
    }

    template<typename T>
    const QVector<ModuleDescription<T>>& getByGroup(
        const QString& group,
        const QHash<QString, QVector<ModuleDescription<T>>>& hash) const
    {
        static const QVector<ModuleDescription<T>> empty;
        auto it = hash.find(group.trimmed());
        if (it == hash.end()) return empty;
        return it.value();
    }

    template<typename T>
    QList<QString> getSortedKeys(const QHash<QString, QVector<ModuleDescription<T>>>& map) const {
        QList<QString> keys = map.keys();
        std::sort(keys.begin(), keys.end(), [](const QString& a, const QString& b) {
            return std::tuple{!a.isEmpty(), a} < std::tuple{!b.isEmpty(), b};
        });
        return keys;
    }
};

#endif // MODULEMANAGER_H
