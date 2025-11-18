// PluginScanner.cpp
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstring>


namespace fs = std::filesystem;

struct PluginInfo {
    std::string filename;
    std::string type;  // "ESM", "ESP", or "ESL"
    std::vector<std::string> masters;
};

bool readTES4Header(const fs::path &filepath, PluginInfo &out) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return false;

    std::string ext = filepath.extension().string();
    for (auto &c : ext) c = tolower(c);
    out.type = (ext == ".esm") ? "ESM" : (ext == ".esl") ? "ESL" : "ESP";
    out.filename = filepath.filename().string();

    // Read entire file into memory
    std::vector<char> buf((std::istreambuf_iterator<char>(file)), {});
    size_t size = buf.size();
    if (size < 16) return true; // too small, probably not valid

    const char *p = buf.data();
    const char *end = p + size;

    // Search for MAST record headers properly
    while (p + 8 < end) {
        if (std::memcmp(p, "MAST", 4) == 0) {
            uint32_t length = *reinterpret_cast<const uint32_t*>(p + 4);
            const char *nameStart = p + 8;
            if (nameStart + length > end) break;

            std::string masterName(nameStart, length);
            // ensure null-terminated trimming
            if (!masterName.empty()) {
                auto nullPos = masterName.find('\0');
                if (nullPos != std::string::npos)
                    masterName.resize(nullPos);
            }
            out.masters.push_back(masterName);

            p += 8 + length; // jump past this record
        } else {
            p++;
        }
    }

    return true;
}

std::vector<PluginInfo> scanPlugins(const std::string &dataPath) {
    std::vector<PluginInfo> plugins;
    for (auto &entry : fs::directory_iterator(dataPath)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        for (auto &c : ext) c = tolower(c);
        if (ext == ".esm" || ext == ".esp" || ext == ".esl") {
            PluginInfo info;
            if (readTES4Header(entry.path(), info)) {
                plugins.push_back(info);
            }
        }
    }
    return plugins;
}

int main() {
    std::string dataDir = R"(/run/media/kartavian/45248133-7999-48d9-8bfd-de9ca71cac60/SteamLibrary/steamapps/common/Skyrim Special Edition/Data/)"; // change this to your test path

    auto plugins = scanPlugins(dataDir);
    std::cout << "Found " << plugins.size() << " plugin(s):\n";
    for (auto &p : plugins) {
        std::cout << "- " << p.filename << " [" << p.type << "]";
        if (!p.masters.empty()) {
            std::cout << "\n  Masters:";
            for (auto &m : p.masters) std::cout << " " << m;
        }
        std::cout << "\n";
    }
}
