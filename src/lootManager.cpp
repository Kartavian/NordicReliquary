#include "lootManager.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>

LootManager::LootManager(const QString &dataPath, const QString &installPath, LootGameType gameType)
{
    qDebug() << "[LOOT] Creating handle. dataPath=" << dataPath << "installPath=" << installPath << "gameType=" << gameType;
    QByteArray data = dataPath.toUtf8();
    QByteArray install = installPath.toUtf8();
    handle = loot_create_game_handle(
        gameType,
        data.constData(),
        install.constData()
    );
    if (!handle) {
        qWarning() << "[LOOT] Failed to create game handle for" << installPath;
    } else {
        qDebug() << "[LOOT] Handle created successfully.";
    }
}

LootManager::~LootManager()
{
    if (handle)
        loot_destroy_game_handle(handle);
}

bool LootManager::sortPlugins()
{
    if (!handle) {
        qWarning() << "[LOOT] sortPlugins called without valid handle.";
        return false;
    }
    int result = loot_sort_plugins(handle);
    return result == 0;
}

bool LootManager::loadMasterlist(const QString &masterlistPath, const QString &preludePath)
{
    if (!handle)
        return false;

    QByteArray masterlistUtf8 = masterlistPath.toUtf8();
    QByteArray preludeUtf8 = preludePath.toUtf8();
    const char *preludePtr = preludeUtf8.isEmpty() ? nullptr : preludeUtf8.constData();
    int rc = loot_load_masterlist(handle, masterlistUtf8.constData(), preludePtr);
    if (rc != 0) {
        qWarning() << "[LOOT] Failed to load masterlist" << masterlistPath << "rc=" << rc;
    }
    return rc == 0;
}

bool LootManager::loadUserlist(const QString &userlistPath)
{
    if (!handle)
        return false;

    QByteArray utf8 = userlistPath.toUtf8();
    int rc = loot_load_userlist(handle, utf8.constData());
    if (rc != 0) {
        qWarning() << "[LOOT] Unable to load userlist" << userlistPath << "rc=" << rc;
    }
    return rc == 0;
}

bool LootManager::clearUserMetadata()
{
    if (!handle)
        return false;

    int rc = loot_clear_user_metadata(handle);
    if (rc != 0) {
        qWarning() << "[LOOT] Failed clearing user metadata rc=" << rc;
    }
    return rc == 0;
}

QJsonObject LootManager::pluginDetails(const QString &pluginName)
{
    if (!handle)
        return QJsonObject();

    QByteArray utf8 = pluginName.toUtf8();
    char *json = loot_get_plugin_details_json(handle, utf8.constData());
    if (!json)
        return QJsonObject();

    QByteArray payload(json);
    loot_free_json(json);

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return QJsonObject();

    return doc.object();
}

QJsonArray LootManager::generalMessages()
{
    if (!handle)
        return QJsonArray();

    char *json = loot_get_general_messages_json(handle);
    if (!json)
        return QJsonArray();

    QByteArray payload(json);
    loot_free_json(json);

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray())
        return QJsonArray();

    return doc.array();
}
