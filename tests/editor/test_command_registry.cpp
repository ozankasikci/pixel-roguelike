#include "editor/debug/CommandRegistry.h"
#include "common/TestSupport.h"

#include <cassert>
#include <stdexcept>
#include <string>

int main() {
    // Test 1: Register a command that returns {"value": 42}, dispatch it,
    // verify response has ok=true and data.value=42.
    {
        CommandRegistry reg;
        reg.registerCommand("get_value", [](const nlohmann::json& /*args*/) -> nlohmann::json {
            return {{"value", 42}};
        });
        nlohmann::json result = reg.dispatch("get_value", {});
        assert(result.value("ok", false) == true);
        assert(result.contains("data"));
        assert(result["data"].value("value", 0) == 42);
    }

    // Test 2: Dispatch an unregistered command name, verify ok=false and error contains "Unknown command".
    {
        CommandRegistry reg;
        nlohmann::json result = reg.dispatch("no_such_cmd", {});
        assert(result.value("ok", true) == false);
        assert(result.contains("error"));
        const std::string err = result["error"].get<std::string>();
        assert(err.find("Unknown command") != std::string::npos);
    }

    // Test 3: Register a command that returns JSON already containing "ok" key,
    // verify the response passes through unchanged (no double-wrapping).
    {
        CommandRegistry reg;
        reg.registerCommand("passthrough", [](const nlohmann::json& /*args*/) -> nlohmann::json {
            return {{"ok", true}, {"custom_field", 99}};
        });
        nlohmann::json result = reg.dispatch("passthrough", {});
        assert(result.value("ok", false) == true);
        assert(result.value("custom_field", 0) == 99);
        // Must NOT be double-wrapped (no nested "data" key)
        assert(!result.contains("data"));
    }

    // Test 4: Register a command handler that throws std::runtime_error("boom"),
    // verify ok=false and error contains "boom".
    {
        CommandRegistry reg;
        reg.registerCommand("throw_cmd", [](const nlohmann::json& /*args*/) -> nlohmann::json {
            throw std::runtime_error("boom");
        });
        nlohmann::json result = reg.dispatch("throw_cmd", {});
        assert(result.value("ok", true) == false);
        assert(result.contains("error"));
        const std::string err = result["error"].get<std::string>();
        assert(err.find("boom") != std::string::npos);
    }

    // Test 5: Register two commands with different names, verify each dispatches
    // to the correct handler (not cross-wired).
    {
        CommandRegistry reg;
        reg.registerCommand("cmd_a", [](const nlohmann::json& /*args*/) -> nlohmann::json {
            return {{"source", "a"}};
        });
        reg.registerCommand("cmd_b", [](const nlohmann::json& /*args*/) -> nlohmann::json {
            return {{"source", "b"}};
        });
        nlohmann::json resultA = reg.dispatch("cmd_a", {});
        nlohmann::json resultB = reg.dispatch("cmd_b", {});
        assert(resultA.value("ok", false) == true);
        assert(resultA["data"].value("source", "") == "a");
        assert(resultB.value("ok", false) == true);
        assert(resultB["data"].value("source", "") == "b");
    }

    // Test 6: Re-register a command name with a new handler, verify the new handler is used.
    {
        CommandRegistry reg;
        reg.registerCommand("overwrite", [](const nlohmann::json& /*args*/) -> nlohmann::json {
            return {{"version", 1}};
        });
        // Overwrite with new handler
        reg.registerCommand("overwrite", [](const nlohmann::json& /*args*/) -> nlohmann::json {
            return {{"version", 2}};
        });
        nlohmann::json result = reg.dispatch("overwrite", {});
        assert(result.value("ok", false) == true);
        assert(result["data"].value("version", 0) == 2);
    }

    return 0;
}
