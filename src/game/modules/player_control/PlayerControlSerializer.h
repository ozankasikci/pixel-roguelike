#pragma once

#include <sstream>
#include <string>
#include <vector>

struct LevelDef;

void parsePlayerSpawn(LevelDef& data,
                      const std::string& path,
                      int lineNumber,
                      const std::vector<std::string>& tokens);

void serializePlayerSpawn(std::ostringstream& out, const LevelDef& data);
