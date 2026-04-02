#include "editor/debug/CommandRegistry.h"

void CommandRegistry::registerCommand(const std::string& cmd, CommandHandler handler) {
    handlers_[cmd] = std::move(handler);
}

nlohmann::json CommandRegistry::dispatch(const std::string& cmd, const nlohmann::json& args) {
    auto it = handlers_.find(cmd);
    if (it == handlers_.end()) {
        return {{"ok", false}, {"error", "Unknown command: " + cmd}};
    }
    try {
        nlohmann::json result = it->second(args);
        if (!result.contains("ok")) {
            return {{"ok", true}, {"data", result}};
        }
        return result;
    } catch (const std::exception& e) {
        return {{"ok", false}, {"error", std::string("Command error: ") + e.what()}};
    }
}
