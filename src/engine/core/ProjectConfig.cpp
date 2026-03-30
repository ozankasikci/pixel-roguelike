#include "engine/core/ProjectConfig.h"

#include <fstream>
#include <string>

std::string readProjectCfgLastScene(const std::string& cfgPath) {
    std::ifstream file(cfgPath);
    if (!file.is_open()) {
        return {};
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("last_scene=", 0) == 0) {
            return line.substr(11); // length of "last_scene="
        }
    }
    return {};
}

void writeProjectCfgLastScene(const std::string& cfgPath, const std::string& sceneFilename) {
    std::ofstream file(cfgPath, std::ios::trunc);
    if (!file.is_open()) {
        return;
    }
    file << "last_scene=" << sceneFilename << "\n";
}
