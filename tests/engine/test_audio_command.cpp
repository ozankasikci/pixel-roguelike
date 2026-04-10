#include <cassert>
#include <cstdio>
#include <vector>

#include "engine/audio/AudioCommand.h"

using namespace engine::audio;

int main() {
    // PlayCommand defaults and field access
    {
        PlayCommand cmd;
        cmd.eventName = "footstep";
        cmd.position = glm::vec3(1.0f, 2.0f, 3.0f);
        cmd.volume = 0.8f;
        cmd.pitch = 1.2f;
        cmd.is3D = true;

        assert(cmd.eventName == "footstep");
        assert(cmd.position.x == 1.0f);
        assert(cmd.position.y == 2.0f);
        assert(cmd.position.z == 3.0f);
        assert(cmd.volume == 0.8f);
        assert(cmd.pitch == 1.2f);
        assert(cmd.is3D == true);
    }

    // PlayLoopingCommand
    {
        PlayLoopingCommand cmd;
        cmd.eventName = "torch_crackle";
        cmd.position = glm::vec3(5.0f, 0.0f, -3.0f);
        cmd.volume = 0.5f;
        cmd.pitch = 1.0f;
        cmd.is3D = true;

        assert(cmd.eventName == "torch_crackle");
        assert(cmd.volume == 0.5f);
    }

    // StopCommand
    {
        StopCommand cmd;
        cmd.handle = VoiceHandle(42, 1);
        cmd.fadeTime = 0.25f;

        assert(cmd.handle.id == 42);
        assert(cmd.handle.generation == 1);
        assert(cmd.fadeTime == 0.25f);
    }

    // UpdateVoicePositionCommand
    {
        UpdateVoicePositionCommand cmd;
        cmd.handle = VoiceHandle(7);
        cmd.position = glm::vec3(10.0f, 0.0f, -5.0f);

        assert(cmd.handle.id == 7);
        assert(cmd.position.x == 10.0f);
    }

    // SetBusVolumeCommand
    {
        SetBusVolumeCommand cmd;
        cmd.busName = "sfx";
        cmd.volume = 0.6f;

        assert(cmd.busName == "sfx");
        assert(cmd.volume == 0.6f);
    }

    // SetBusMuteCommand
    {
        SetBusMuteCommand cmd;
        cmd.busName = "music";
        cmd.mute = true;

        assert(cmd.busName == "music");
        assert(cmd.mute == true);
    }

    // SetListenerCommand
    {
        SetListenerCommand cmd;
        cmd.position = glm::vec3(0.0f, 1.7f, 0.0f);
        cmd.forward = glm::vec3(0.0f, 0.0f, -1.0f);
        cmd.up = glm::vec3(0.0f, 1.0f, 0.0f);

        assert(cmd.position.y == 1.7f);
        assert(cmd.forward.z == -1.0f);
        assert(cmd.up.y == 1.0f);
    }

    // SetReverbPresetCommand
    {
        SetReverbPresetCommand cmd;
        cmd.presetName = "large_hall";

        assert(cmd.presetName == "large_hall");
    }

    // Variant holds_alternative for each type
    {
        AudioCommand cmd1 = PlayCommand{.eventName = "hit"};
        assert(std::holds_alternative<PlayCommand>(cmd1));

        AudioCommand cmd2 = PlayLoopingCommand{.eventName = "ambient"};
        assert(std::holds_alternative<PlayLoopingCommand>(cmd2));

        AudioCommand cmd3 = StopCommand{.handle = VoiceHandle(1)};
        assert(std::holds_alternative<StopCommand>(cmd3));

        AudioCommand cmd4 = UpdateVoicePositionCommand{.handle = VoiceHandle(2)};
        assert(std::holds_alternative<UpdateVoicePositionCommand>(cmd4));

        AudioCommand cmd5 = SetBusVolumeCommand{.busName = "master", .volume = 0.9f};
        assert(std::holds_alternative<SetBusVolumeCommand>(cmd5));

        AudioCommand cmd6 = SetBusMuteCommand{.busName = "sfx", .mute = false};
        assert(std::holds_alternative<SetBusMuteCommand>(cmd6));

        AudioCommand cmd7 = SetListenerCommand{};
        assert(std::holds_alternative<SetListenerCommand>(cmd7));

        AudioCommand cmd8 = SetReverbPresetCommand{.presetName = "cave"};
        assert(std::holds_alternative<SetReverbPresetCommand>(cmd8));
    }

    // Variant get access
    {
        AudioCommand cmd = PlayCommand{.eventName = "door_open", .volume = 0.7f};
        auto& play = std::get<PlayCommand>(cmd);
        assert(play.eventName == "door_open");
        assert(play.volume == 0.7f);
    }

    // Command queue with vector swap pattern
    {
        std::vector<AudioCommand> producer_queue;
        std::vector<AudioCommand> consumer_queue;

        // Producer fills the queue
        producer_queue.push_back(PlayCommand{.eventName = "step_stone", .volume = 0.5f});
        producer_queue.push_back(
            SetListenerCommand{.position = glm::vec3(1.0f, 1.7f, 0.0f)});
        producer_queue.push_back(StopCommand{.handle = VoiceHandle(3), .fadeTime = 0.1f});

        assert(producer_queue.size() == 3);
        assert(consumer_queue.empty());

        // Swap — producer gets empty queue, consumer gets commands
        std::swap(producer_queue, consumer_queue);

        assert(producer_queue.empty());
        assert(consumer_queue.size() == 3);

        // Consumer processes commands
        assert(std::holds_alternative<PlayCommand>(consumer_queue[0]));
        assert(std::holds_alternative<SetListenerCommand>(consumer_queue[1]));
        assert(std::holds_alternative<StopCommand>(consumer_queue[2]));

        auto& play = std::get<PlayCommand>(consumer_queue[0]);
        assert(play.eventName == "step_stone");

        consumer_queue.clear();
        assert(consumer_queue.empty());
    }

    std::printf("test_audio_command: all assertions passed\n");
    return 0;
}
