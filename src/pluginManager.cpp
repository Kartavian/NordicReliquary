#include "../ui/pluginManager.h"
#include <iostream>
#include <fstream>
#include <cstring>

namespace fs = std::filesystem;

bool PluginManager::readTES4Header(const fs::path& filepath, PluginInfo& out) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return false;

    // Set basic file info
    std::string ext = filepath.extension().string();
    for (auto& c : ext) c = tolower(c);
    if (ext == ".esm") out.type = "ESM";
    else if (ext == ".esl") out.type = "ESL";
    else out.type = "ESP";
    out.filename = filepath.filename().string();

    // Read full file
    std::vector<char> buf((std::istreambuf_iterator<char>(file)), {});
    size_t size = buf.size();
    if (size < 16) return true; // too small to contain a TES4 header

    const char* p = buf.data();
    const char* end = p + size;

    // Parse binary records
    while (p + 8 < end) {
        if (std::memcmp(p, "MAST", 4) == 0) {
            uint32_t length = *reinterpret_cast<const uint32_t*>(p + 4);
            const char* nameStart = p + 8;
            if (nameStart + length > end) break;

            std::string masterName(nameStart, length);
            if (!masterName.empty()) {
                auto nullPos = masterName.find('\0');
                if (nullPos != std::string::npos)
                    masterName.resize(nullPos);
            }
            out.masters.push_back(masterName);
            p += 8 + length;
        } else {
            p++;
        }
    }
    return true;
}

void PluginManager::scan(const std::string& dataDir) {
    std::cout << "[PluginManager] scan start: " << dataDir << std::endl;
    plugins.clear();

    if (!fs::exists(dataDir) || !fs::is_directory(dataDir)) {
        std::cerr << "Error: directory not found: " << dataDir << std::endl;
        return;
    }

    for (const auto& entry : fs::directory_iterator(dataDir)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        for (auto& c : ext) c = tolower(c);
        if (ext == ".esm" || ext == ".esp" || ext == ".esl") {
            std::cout << "[PluginManager] Reading " << entry.path() << std::endl;
            PluginInfo info;
            if (readTES4Header(entry.path(), info)) {
                info.syncName();
                plugins.push_back(info);
            }
        }
    }
    std::cout << "[PluginManager] scan complete. Total: " << plugins.size() << std::endl;
}

void PluginManager::printSummary() const {
    std::cout << "Found " << plugins.size() << " plugin(s):\n";
    for (const auto& p : plugins) {
        std::cout << "- " << p.filename << " [" << p.type << "]";
        if (!p.masters.empty()) {
            std::cout << "\n  Masters:";
            for (const auto& m : p.masters)
                std::cout << " " << m;
        }
        std::cout << "\n";
    }
}
