#include <stdio.h>
#include "include/loot_shim.h"

int main() {
    const char* data_path = "/mnt/skyrimae/SteamLibrary/steamapps/common/Skyrim/Data";
    const char* install_path = "/mnt/skyrimae/SteamLibrary/steamapps/common/Skyrim";

    GameType game = GameType_Tes5;

    printf("Using data path: %s\n", data_path);

    LootGameHandle* h = loot_create_game_handle(game, data_path, install_path);

    printf("Sorting result: %d\n", loot_sort_plugins(h));

    char* plugin_json = loot_get_plugin_details_json(h, "Skyrim.esm");
    if (plugin_json) {
        printf("Plugin metadata: %s\n", plugin_json);
        loot_free_json(plugin_json);
    }

    char* general_json = loot_get_general_messages_json(h);
    if (general_json) {
        printf("General messages: %s\n", general_json);
        loot_free_json(general_json);
    }
    loot_destroy_game_handle(h);

    return 0;
}
