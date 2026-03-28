#include "editor/scene/EditorSceneSerializer.h"

namespace EditorSceneSerializer {

LevelDef load(const std::string& path) {
    return loadLevelDef(path);
}

void save(const std::string& path, const LevelDef& level) {
    saveLevelDef(path, level);
}

} // namespace EditorSceneSerializer
