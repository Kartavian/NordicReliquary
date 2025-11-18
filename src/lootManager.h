#ifndef LOOTMANAGER_H
#define LOOTMANAGER_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include "../loot-shim/include/loot_shim.h"

class LootManager {
public:
    LootManager(const QString &dataPath, const QString &installPath, LootGameType gameType);
    ~LootManager();

    bool sortPlugins();
    bool isValid() const { return handle != nullptr; }
    bool loadMasterlist(const QString &masterlistPath, const QString &preludePath = QString());
    bool loadUserlist(const QString &userlistPath);
    bool clearUserMetadata();
    QJsonObject pluginDetails(const QString &pluginName);
    QJsonArray generalMessages();

private:
    LootGameHandle *handle = nullptr;
};

#endif
