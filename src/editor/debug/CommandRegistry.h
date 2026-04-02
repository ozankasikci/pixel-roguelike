#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

using CommandHandler = std::function<nlohmann::json(const nlohmann::json& args)>;

class CommandRegistry {
public:
    CommandRegistry() = default;
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;

    void registerCommand(const std::string& cmd, CommandHandler handler);
    nlohmann::json dispatch(const std::string& cmd, const nlohmann::json& args);

private:
    std::unordered_map<std::string, CommandHandler> handlers_;
};
