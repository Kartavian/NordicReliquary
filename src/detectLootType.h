#ifndef DETECTLOOTTYPE_H
#define DETECTLOOTTYPE_H

#include <QString>
#include "../loot-shim/include/loot_shim.h"

LootGameType detectLootType(const QString &gameDir);

#endif
