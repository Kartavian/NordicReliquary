#include "detectLootType.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>

LootGameType detectLootType(const QString &dir)
{
    qDebug() << "[DetectLOOT] Checking game directory:" << dir;

    // --- Morrowind ---
    if (QFileInfo(dir + "/Morrowind.exe").exists())
        return LootGameType_Morrowind;

    // --- Oblivion ---
    if (QFileInfo(dir + "/Oblivion.exe").exists())
        return LootGameType_Oblivion;

    // --- Skyrim Legendary Edition (Oldrim) ---
    if (QFileInfo(dir + "/TESV.exe").exists())
        return LootGameType_Skyrim;

    // --- Skyrim SE / AE ---
    // AE still uses SkyrimSE.exe
    if (QFileInfo(dir + "/SkyrimSE.exe").exists())
        return LootGameType_SkyrimSE;

    // --- Fallout 3 ---
    if (QFileInfo(dir + "/Fallout3.exe").exists())
        return LootGameType_Fallout3;

    // --- Fallout New Vegas ---
    if (QFileInfo(dir + "/FalloutNV.exe").exists() ||
        QFileInfo(dir + "/FalloutNVLauncher.exe").exists())
        return LootGameType_FalloutNV;

    // --- Fallout 4 ---
    if (QFileInfo(dir + "/Fallout4.exe").exists())
        return LootGameType_Fallout4;

    // --- OpenMW ---
    if (QFileInfo(dir + "/openmw.cfg").exists())
        return LootGameType_OpenMW;

    // --- Default Fallback: Skyrim SE (safest LOOT behavior) ---
    qWarning() << "[DetectLOOT] Unknown game type. Defaulting to SkyrimSE.";
    return LootGameType_SkyrimSE;
}
