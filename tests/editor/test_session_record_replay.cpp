#include "editor/debug/SessionRecorder.h"
#include "editor/debug/SessionPlayer.h"
#include "editor/debug/CommandRegistry.h"
#include "common/TestSupport.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    const auto tmpDir = test_support::resetTempDirectory("debug_harness_tests");

    // Test 1: Recorder start/stop lifecycle.
    {
        SessionRecorder rec;
        assert(!rec.isRecording());

        const std::string filePath = (tmpDir / "session1.editsession").string();
        nlohmann::json startResult = rec.start(filePath, "test.scene");
        assert(startResult.value("ok", false) == true);
        assert(rec.isRecording());

        nlohmann::json stopResult = rec.stop();
        assert(stopResult.value("ok", false) == true);
        assert(!rec.isRecording());
        assert(std::filesystem::exists(filePath));
    }

    // Test 2: Recorder writes valid JSON with required fields and initial snapshot.
    {
        SessionRecorder rec;
        const std::string filePath = (tmpDir / "session2.editsession").string();
        rec.start(filePath, "my.scene");
        rec.stop();

        std::ifstream in(filePath);
        assert(in.is_open());
        nlohmann::json doc = nlohmann::json::parse(in);

        assert(doc.contains("version"));
        assert(doc.contains("scene"));
        assert(doc.contains("duration_ms"));
        assert(doc.contains("events"));
        assert(doc["events"].is_array());
        assert(!doc["events"].empty());

        // First event must be initial snapshot at t=0
        const nlohmann::json& firstEvent = doc["events"][0];
        assert(firstEvent.value("type", "") == "snapshot");
        assert(firstEvent.value("t", -1LL) == 0LL);
    }

    // Test 3: Recorder captures commands.
    {
        SessionRecorder rec;
        const std::string filePath = (tmpDir / "session3.editsession").string();
        rec.start(filePath, "cmd.scene");
        rec.recordCommand("test.cmd", {{"x", 1}});
        rec.recordCommand("test.cmd", {{"x", 2}});
        rec.stop();

        std::ifstream in(filePath);
        assert(in.is_open());
        nlohmann::json doc = nlohmann::json::parse(in);
        assert(doc["events"].size() == 3); // 1 initial snapshot + 2 commands

        const nlohmann::json& cmdEvent = doc["events"][1];
        assert(cmdEvent.value("type", "") == "command");
        assert(cmdEvent.contains("cmd"));
        assert(cmdEvent.contains("args"));
        assert(cmdEvent.contains("t"));
        assert(cmdEvent.value("t", -1LL) >= 0LL);
    }

    // Test 4: Recorder captures snapshots.
    {
        SessionRecorder rec;
        const std::string filePath = (tmpDir / "session4.editsession").string();
        rec.start(filePath, "snap.scene");
        rec.recordSnapshot({{"selection", "box"}});
        rec.stop();

        std::ifstream in(filePath);
        assert(in.is_open());
        nlohmann::json doc = nlohmann::json::parse(in);
        assert(doc["events"].size() == 2); // 1 initial snapshot + 1 explicit snapshot

        const nlohmann::json& snapEvent = doc["events"][1];
        assert(snapEvent.value("type", "") == "snapshot");
    }

    // Test 5: Recorder rejects double-start.
    {
        SessionRecorder rec;
        const std::string filePath = (tmpDir / "session5.editsession").string();
        rec.start(filePath, "test.scene");
        nlohmann::json secondStart = rec.start((tmpDir / "session5b.editsession").string(), "test.scene");
        assert(secondStart.value("ok", true) == false);
        assert(secondStart.contains("error"));
        const std::string err = secondStart["error"].get<std::string>();
        assert(err.find("Already recording") != std::string::npos);
        rec.stop();
    }

    // Test 6: Recorder rejects stop-when-not-recording.
    {
        SessionRecorder rec;
        nlohmann::json stopResult = rec.stop();
        assert(stopResult.value("ok", true) == false);
        assert(stopResult.contains("error"));
    }

    // Test 7: Recorder ignores events when not recording.
    {
        SessionRecorder rec;
        rec.recordCommand("early.cmd", {{"x", 0}}); // Should not crash, should be ignored

        const std::string filePath = (tmpDir / "session7.editsession").string();
        rec.start(filePath, "test.scene");
        rec.stop();

        std::ifstream in(filePath);
        assert(in.is_open());
        nlohmann::json doc = nlohmann::json::parse(in);
        // Only the initial snapshot should exist
        assert(doc["events"].size() == 1);
    }

    // Test 8: Player load.
    {
        const std::string filePath = (tmpDir / "player_session.editsession").string();
        nlohmann::json doc = {
            {"version", 1},
            {"scene", "test.scene"},
            {"duration_ms", 100},
            {"events", nlohmann::json::array({
                {{"t", 0}, {"type", "snapshot"}, {"data", {{"scene", "test.scene"}}}},
                {{"t", 50}, {"type", "command"}, {"cmd", "echo"}, {"args", nlohmann::json::object()}}
            })}
        };
        std::ofstream out(filePath);
        out << doc.dump(2);
        out.close();

        SessionPlayer player;
        nlohmann::json loadResult = player.load(filePath);
        assert(loadResult.value("ok", false) == true);
        assert(loadResult.value("events", 0) == 2);
    }

    // Test 9: Player step.
    {
        const std::string filePath = (tmpDir / "player_step.editsession").string();
        nlohmann::json doc = {
            {"version", 1},
            {"scene", "test.scene"},
            {"duration_ms", 100},
            {"events", nlohmann::json::array({
                {{"t", 0}, {"type", "snapshot"}, {"data", {{"scene", "test.scene"}}}},
                {{"t", 50}, {"type", "command"}, {"cmd", "echo"}, {"args", nlohmann::json::object()}}
            })}
        };
        std::ofstream out(filePath);
        out << doc.dump(2);
        out.close();

        bool echoCalled = false;
        CommandRegistry reg;
        reg.registerCommand("echo", [&echoCalled](const nlohmann::json& /*args*/) -> nlohmann::json {
            echoCalled = true;
            return {{"ok", true}};
        });

        SessionPlayer player;
        player.load(filePath);

        // First step: snapshot event, echo should NOT be called yet
        nlohmann::json step1 = player.step(reg);
        assert(step1.value("ok", false) == true);
        assert(!echoCalled);

        // Second step: command event, echo should now be called
        nlohmann::json step2 = player.step(reg);
        assert(step2.value("ok", false) == true);
        assert(echoCalled);
    }

    // Test 10: Player load missing file.
    {
        SessionPlayer player;
        nlohmann::json result = player.load((tmpDir / "nonexistent.file").string());
        assert(result.value("ok", true) == false);
        assert(result.contains("error"));
    }

    // Test 11: Player play without load.
    {
        SessionPlayer player;
        nlohmann::json result = player.play();
        assert(result.value("ok", true) == false);
        assert(result.contains("error"));
    }

    return 0;
}
