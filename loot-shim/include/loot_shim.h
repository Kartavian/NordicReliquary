#ifndef LOOT_SHIM_H
#define LOOT_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------
// LOOT-Compatible Game Types (must match Rust + LOOT names)
// ---------------------------------------------------------
typedef enum {
    LootGameType_Morrowind = 0,
    LootGameType_Oblivion  = 1,
    LootGameType_Skyrim    = 2,  // Skyrim LE
    LootGameType_SkyrimSE  = 3,  // Skyrim SE / AE
    LootGameType_Fallout3  = 4,
    LootGameType_FalloutNV = 5,
    LootGameType_Fallout4  = 6,
    LootGameType_OpenMW    = 7
} LootGameType;

// ---------------------------------------------------------
// Opaque handle from Rust (forward declaration)
// ---------------------------------------------------------
typedef struct LootGameHandle LootGameHandle;

// ---------------------------------------------------------
// Shim API exposed to C/C++
// ---------------------------------------------------------

LootGameHandle* loot_create_game_handle(
    LootGameType game,
    const char* data_path,
    const char* install_path
);

void loot_destroy_game_handle(LootGameHandle* handle);

int loot_sort_plugins(LootGameHandle* handle);
int loot_load_masterlist(LootGameHandle* handle,
                        const char* masterlist_path,
                        const char* prelude_path);

int loot_load_userlist(LootGameHandle* handle, const char* userlist_path);

int loot_clear_user_metadata(LootGameHandle* handle);

char* loot_get_plugin_details_json(LootGameHandle* handle, const char* plugin_name);

char* loot_get_general_messages_json(LootGameHandle* handle);

void loot_free_json(char* json);

#ifdef __cplusplus
}
#endif

#endif // LOOT_SHIM_H
