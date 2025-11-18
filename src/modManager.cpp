#include "modManager.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QDebug>

namespace {
QString readFileBaseName(const QString &path) {
    return QFileInfo(path).completeBaseName();
}
}

ModManager::ModManager(const QString &workspacePath,
                       const QString &gameInstallPath,
                       const QString &virtualDataPath,
                       QObject *parent)
    : QObject(parent),
      workspace(workspacePath),
      gameInstall(gameInstallPath),
      virtualData(virtualDataPath)
{
    modsRoot = workspace + "/Mods";
    downloadsPath = workspace + "/Downloads";
    registryPath = workspace + "/mods.json";
}

void ModManager::setDownloadsRoot(const QString &path)
{
    downloadsPath = path;
}

bool ModManager::initialize(QString *errorMessage)
{
    ensureDirectories();

    if (!copyBasePlugins(errorMessage))
        return false;

    if (!loadRegistry())
        return false;

    // Ensure enabled mods have their plugins in the virtual data folder
    for (ModRecord &record : installedMods) {
        if (record.enabled)
            copyPluginsToVirtual(record, nullptr);
        if (record.type == ModType::ToolMod) {
            deployToolAssets(record, nullptr);
        }
    }
    saveRegistry();
    return true;
}

void ModManager::ensureDirectories() const
{
    QDir().mkpath(workspace);
    QDir().mkpath(modsRoot);
    QDir().mkpath(downloadsPath);
    QDir().mkpath(virtualData);
}

bool ModManager::copyBasePlugins(QString *errorMessage)
{
    if (gameInstall.isEmpty())
        return true;

    QDir dataDir(gameInstall + "/Data");
    if (!dataDir.exists()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Game Data folder not found: %1").arg(dataDir.absolutePath());
        return false;
    }

    QStringList filters = {"*.esm", "*.esp", "*.esl"};
    dataDir.setNameFilters(filters);
    for (const QFileInfo &info : dataDir.entryInfoList(QDir::Files)) {
        QString destPath = virtualData + "/" + info.fileName();
        if (!QFile::exists(destPath))
            QFile::copy(info.absoluteFilePath(), destPath);
    }
    return true;
}

bool ModManager::loadRegistry()
{
    installedMods.clear();

    QFile f(registryPath);
    if (!f.exists())
        return true;

    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[ModManager] Failed to open registry:" << registryPath;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray())
        return true;

    for (const QJsonValue &value : doc.array()) {
        QJsonObject obj = value.toObject();
        ModRecord record;
        record.id = obj.value("id").toString();
        record.name = obj.value("name").toString();
        record.archiveName = obj.value("archive").toString();
        record.modPath = obj.value("modPath").toString();
        record.dataPath = obj.value("dataPath").toString();
        record.enabled = obj.value("enabled").toBool(true);
        QString typeStr = obj.value("type").toString();
        if (typeStr == "tool")
            record.type = ModType::ToolMod;
        else
            record.type = ModType::PluginMod;
        record.launcherPath = obj.value("launcherPath").toString();
        record.launcherArgs = obj.value("launcherArgs").toString();
        QJsonArray pluginArray = obj.value("plugins").toArray();
        for (const QJsonValue &p : pluginArray)
            record.pluginFiles << p.toString();

        if (record.type == ModType::ToolMod) {
            if (record.launcherPath.isEmpty()) {
                QStringList loaderNames = {"skse64_loader.exe", "skse_loader.exe"};
                QString base = workspace + "/Tools/" + record.id + "/";
                for (const QString &name : loaderNames) {
                    QString candidate = base + name;
                    if (QFileInfo::exists(candidate)) {
                        record.launcherPath = candidate;
                        break;
                    }
                }
                if (record.launcherPath.isEmpty() && !loaderNames.isEmpty())
                    record.launcherPath = base + loaderNames.first();
            }
        }

        if (!record.id.isEmpty())
            installedMods.push_back(record);
    }
    return true;
}

bool ModManager::saveRegistry() const
{
    QJsonArray arr;
    for (const ModRecord &record : installedMods) {
        QJsonObject obj;
        obj.insert("id", record.id);
        obj.insert("name", record.name);
        obj.insert("archive", record.archiveName);
        obj.insert("modPath", record.modPath);
        obj.insert("dataPath", record.dataPath);
        obj.insert("enabled", record.enabled);
        obj.insert("type", record.type == ModType::ToolMod ? "tool" : "mod");
        obj.insert("launcherPath", record.launcherPath);
        obj.insert("launcherArgs", record.launcherArgs);
        QJsonArray plugins;
        for (const QString &plugin : record.pluginFiles)
            plugins.append(plugin);
        obj.insert("plugins", plugins);
        arr.append(obj);
    }

    QFile f(registryPath);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "[ModManager] Failed to write registry:" << registryPath;
        return false;
    }
    f.write(QJsonDocument(arr).toJson());
    return true;
}

QString ModManager::sanitizeName(const QString &archivePath) const
{
    QString base = readFileBaseName(archivePath);
    base.replace(' ', '_');
    base.replace('/', '_');
    base.replace('\\', '_');
    return base;
}

bool ModManager::extractArchive(const QString &archivePath,
                                const QString &destination,
                                QString *errorMessage) const
{
    QDir destDir(destination);
    if (destDir.exists())
        destDir.removeRecursively();
    QDir().mkpath(destination);

    QProcess proc;
    QStringList args = {"x", archivePath, QString("-o%1").arg(destination), "-y"};
    proc.start("7z", args);
    if (!proc.waitForStarted()) {
        if (errorMessage)
            *errorMessage = "Failed to start 7z process.";
        return false;
    }
    proc.waitForFinished(-1);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (errorMessage)
            *errorMessage = QString("7z extraction failed: %1").arg(QString::fromLocal8Bit(proc.readAllStandardError()));
        return false;
    }
    return true;
}

QString ModManager::resolveDataFolder(const QString &modRoot) const
{
    QDir modDir(modRoot);
    if (modDir.exists("Data"))
        return modDir.absoluteFilePath("Data");

    // search for case-insensitive "data"
    for (const QFileInfo &info : modDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (info.fileName().compare("Data", Qt::CaseInsensitive) == 0)
            return info.absoluteFilePath();
    }

    // else, create Data and move contents
    QString dataPath = modDir.absoluteFilePath("Data");
    QDir().mkpath(dataPath);

    for (const QFileInfo &info : modDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
        if (info.fileName().compare("Data", Qt::CaseInsensitive) == 0)
            continue;

        QString target = dataPath + "/" + info.fileName();
        QFile::rename(info.absoluteFilePath(), target);
    }
    return dataPath;
}

QStringList ModManager::findPluginFiles(const QString &dataDir) const
{
    QStringList plugins;
    QDirIterator it(dataDir, {"*.esm", "*.esp", "*.esl"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        plugins << QFileInfo(it.filePath()).fileName();
    }
    return plugins;
}

bool ModManager::installArchive(const QString &archivePath,
                                ModRecord *outRecord,
                                QString *errorMessage)
{
    QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Archive not found: %1").arg(archivePath);
        return false;
    }

    QString modId = sanitizeName(archiveInfo.fileName());
    QString baseId = modId;
    int suffix = 1;
    while (QDir(modsRoot + "/" + modId).exists()) {
        modId = QString("%1_%2").arg(baseId).arg(suffix++);
    }
    QString modFolder = modsRoot + "/" + modId;

    if (!extractArchive(archivePath, modFolder, errorMessage))
        return false;

    QString dataDir = resolveDataFolder(modFolder);
    QStringList plugins = findPluginFiles(dataDir);

    ModRecord record;
    record.id = modId;
    record.name = readFileBaseName(archiveInfo.fileName());
    record.archiveName = archiveInfo.fileName();
    record.modPath = modFolder;
    record.dataPath = dataDir;
    record.pluginFiles = plugins;
    record.enabled = true;
    if (record.name.contains("skse", Qt::CaseInsensitive)) {
        record.type = ModType::ToolMod;
        // determine probable loader name
        QString loaderCandidate;
        QStringList loaderNames = {"skse64_loader.exe", "skse_loader.exe"};
        for (const QString &name : loaderNames) {
            if (QFileInfo::exists(record.modPath + "/" + name)) {
                loaderCandidate = name;
                break;
            }
        }
        if (loaderCandidate.isEmpty())
            loaderCandidate = loaderNames.first();
        record.launcherPath = workspace + "/Tools/" + record.id + "/" + loaderCandidate;
    }

    installedMods.push_back(record);
    if (record.type == ModType::ToolMod)
        deployToolAssets(installedMods.back(), nullptr);

    if (!saveRegistry()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to save mod registry.");
    }

    copyPluginsToVirtual(record, nullptr);
    emit modsChanged();

    if (outRecord)
        *outRecord = record;
    return true;
}

bool ModManager::copyPluginsToVirtual(const ModRecord &record, QString *errorMessage)
{
    for (const QString &plugin : record.pluginFiles) {
        QString src = record.dataPath + "/" + plugin;
        QString dest = virtualData + "/" + plugin;
        if (QFile::exists(dest))
            QFile::remove(dest);
        if (!QFile::copy(src, dest)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Failed to copy plugin %1").arg(plugin);
            return false;
        }
    }
    return true;
}

bool ModManager::removePluginsFromVirtual(const ModRecord &record, QString *errorMessage)
{
    for (const QString &plugin : record.pluginFiles) {
        QString dest = virtualData + "/" + plugin;
        if (QFile::exists(dest) && !QFile::remove(dest)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Failed to remove plugin %1").arg(plugin);
            return false;
        }
    }
    return true;
}

bool ModManager::setModEnabled(const QString &modId, bool enabled, QString *errorMessage)
{
    for (ModRecord &record : installedMods) {
        if (record.id != modId)
            continue;

        if (record.enabled == enabled)
            return true;

        record.enabled = enabled;
        bool ok = enabled ? copyPluginsToVirtual(record, errorMessage)
                          : removePluginsFromVirtual(record, errorMessage);

        if (ok && record.type == ModType::ToolMod) {
            if (enabled)
                deployToolAssets(record, nullptr);
            else
                cleanupToolAssets(record);
        }

        saveRegistry();
        if (ok)
            emit modsChanged();
        return ok;
    }

    if (errorMessage)
        *errorMessage = QStringLiteral("Unknown mod id: %1").arg(modId);
    return false;
}

bool ModManager::removeMod(const QString &modId, QString *errorMessage)
{
    for (int i = 0; i < installedMods.size(); ++i) {
        const ModRecord &record = installedMods.at(i);
        if (record.id != modId)
            continue;

        removePluginsFromVirtual(record, nullptr);
        cleanupToolAssets(record);

        QDir dir(record.modPath);
        if (dir.exists() && !dir.removeRecursively()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Failed to delete mod folder: %1").arg(record.modPath);
            return false;
        }

        installedMods.remove(i);
        saveRegistry();
        emit modsChanged();
        return true;
    }

    if (errorMessage)
        *errorMessage = QStringLiteral("Unknown mod id: %1").arg(modId);
    return false;
}

bool ModManager::deployToolAssets(ModRecord &record, QString *errorMessage)
{
    if (record.type != ModType::ToolMod)
        return true;

    QString toolsRoot = workspace + "/Tools/" + record.id;
    QDir().mkpath(toolsRoot);

    qDebug() << "[ModManager] Deploying tool assets for" << record.name
             << "from" << record.modPath << "to" << toolsRoot;

    bool copiedAny = false;
    QDir root(record.modPath);
    QString loaderSource;
    QDirIterator it(record.modPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFileInfo info(it.next());
        QString file = info.fileName();
        if (!file.startsWith("skse", Qt::CaseInsensitive))
            continue;
        QString suffix = info.suffix().toLower();
        if (!(suffix == "exe" || suffix == "dll" || suffix == "txt"))
            continue;
        QString src = info.absoluteFilePath();
        QString dest = toolsRoot + "/" + file;
        QDir().mkpath(QFileInfo(dest).absolutePath());
        QFile::remove(dest);
        if (QFile::copy(src, dest)) {
            copiedAny = true;
            if (suffix == "exe" && file.contains("loader", Qt::CaseInsensitive)) {
                loaderSource = dest;
                record.launcherPath = dest;
            }
        }
    }

    if (!copiedAny && errorMessage)
        *errorMessage = QStringLiteral("No SKSE files were copied.");

    qDebug() << "[ModManager] Deployment complete. Launcher path:" << record.launcherPath;
    return copiedAny;
}

void ModManager::cleanupToolAssets(const ModRecord &record)
{
    if (record.type != ModType::ToolMod)
        return;

    QDir toolsDir(workspace + "/Tools/" + record.id);
    if (toolsDir.exists())
        toolsDir.removeRecursively();
}
