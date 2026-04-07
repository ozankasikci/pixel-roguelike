#pragma once

#include <string>
#include <vector>
#include <sstream>

struct LevelDef;

// Parse a door_group record from the scene file.
// tokens: the tokenized fields after the "door_group" keyword (from collectRemainingTokens).
// path/lineNumber: for error reporting.
void parseDoorGroup(LevelDef& data,
                    const std::string& path,
                    int lineNumber,
                    const std::vector<std::string>& tokens);

// Serialize all door groups in data to the output stream.
void serializeDoorGroups(std::ostringstream& out, const LevelDef& data);
