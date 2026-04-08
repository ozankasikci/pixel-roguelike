#pragma once

#include <string>
#include <vector>
#include <sstream>

struct LevelDef;

void parseCheckpoint(LevelDef& data,
                     const std::string& path,
                     int lineNumber,
                     const std::vector<std::string>& tokens);

void serializeCheckpoints(std::ostringstream& out, const LevelDef& data);
