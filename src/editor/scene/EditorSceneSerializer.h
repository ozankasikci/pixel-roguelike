#pragma once

#include "game/level/LevelDef.h"

#include <string>

namespace EditorSceneSerializer {

LevelDef load(const std::string& path);
void save(const std::string& path, const LevelDef& level);

} // namespace EditorSceneSerializer
