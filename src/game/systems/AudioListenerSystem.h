#pragma once
#include "engine/core/System.h"

#include "engine/ecs/GameRegistry.h"

class Application;
class AudioSystem;

// Free-function accessor for AudioSystem from Application's service locator (per D-10).
// Stores AudioSystem as AudioSystem* in the service locator since AudioSystem is non-copyable.
// Matches existing pattern: game systems access engine subsystems via free functions.
AudioSystem& audioSystem(Application& app);

// Free-function to sync audio listener position from CameraManager (per D-15).
class CameraManager;
void updateAudioListener(const CameraManager& cameraManager, AudioSystem& audio);

// System wrapper for registration via addSystem (matches PlayerMovementSystem/CameraSystem pattern).
// Holds a reference to AudioSystem obtained from Application's system lifecycle.
class AudioListenerSystem : public System {
public:
    explicit AudioListenerSystem(AudioSystem& audio);
    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    AudioSystem& audio_;
};
