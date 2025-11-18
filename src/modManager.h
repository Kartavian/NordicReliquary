#ifndef MODMANAGER_H
#define MODMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

enum class ModType {
    PluginMod,
    ToolMod
};

struct ModRecord {
    QString id;
    QString name;
    QString archiveName;
    QString modPath;
    QString dataPath;
    QStringList pluginFiles;
    bool enabled = true;
    ModType type = ModType::PluginMod;
    QString launcherPath;
    QString launcherArgs;
};

class ModManager : public QObject
{
    Q_OBJECT
public:
    explicit ModManager(const QString &workspacePath,
                        const QString &gameInstallPath,
                        const QString &virtualDataPath,
                        QObject *parent = nullptr);

    bool initialize(QString *errorMessage = nullptr);

    const QVector<ModRecord>& mods() const { return installedMods; }

    bool installArchive(const QString &archivePath,
                        ModRecord *outRecord,
                        QString *errorMessage);

    bool setModEnabled(const QString &modId, bool enabled, QString *errorMessage = nullptr);
    bool removeMod(const QString &modId, QString *errorMessage = nullptr);

    QString downloadsRoot() const { return downloadsPath; }
    QString modsRootPath() const { return modsRoot; }
    QString virtualDataRoot() const { return virtualData; }

    void setDownloadsRoot(const QString &path);

signals:
    void modsChanged();

private:
    QString workspace;
    QString gameInstall;
    QString modsRoot;
    QString downloadsPath;
    QString virtualData;
    QString registryPath;

    QVector<ModRecord> installedMods;

    bool loadRegistry();
    bool saveRegistry() const;

    bool copyBasePlugins(QString *errorMessage);
    void ensureDirectories() const;

    QString sanitizeName(const QString &archivePath) const;
    bool extractArchive(const QString &archivePath,
                        const QString &destination,
                        QString *errorMessage) const;
    QString resolveDataFolder(const QString &modRoot) const;
    QStringList findPluginFiles(const QString &dataDir) const;
    bool copyPluginsToVirtual(const ModRecord &record, QString *errorMessage);
    bool removePluginsFromVirtual(const ModRecord &record, QString *errorMessage);
    bool deployToolAssets(ModRecord &record, QString *errorMessage);
    void cleanupToolAssets(const ModRecord &record);
};

#endif // MODMANAGER_H
