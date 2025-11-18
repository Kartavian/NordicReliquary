#pragma once
#include <string>
#include <vector>
#include <filesystem>

struct PluginInfo {
    std::string filename;                 // Original field used by pluginManager.cpp
    std::string name;                     // For UI readability (alias)
    std::string type;                     // ESM / ESP / ESL
    std::vector<std::string> masters;     // Dependencies (used in readTES4Header)

    PluginInfo() = default;

    // Simple helper to copy filename to name
    void syncName() {
        if (name.empty() && !filename.empty())
            name = filename;
    }
};

class PluginManager {
public:
    PluginManager() = default;

    void scan(const std::string& dataDir);
    const std::vector<PluginInfo>& getPlugins() const { return plugins; }
    void printSummary() const;

private:
    std::vector<PluginInfo> plugins;
    bool readTES4Header(const std::filesystem::path& filepath, PluginInfo& out);
};
