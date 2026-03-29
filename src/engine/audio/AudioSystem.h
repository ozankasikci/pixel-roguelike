#pragma once
#include "engine/core/System.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <cstdint>

class AudioSystem : public System {
public:
    AudioSystem();
    ~AudioSystem() override;

    // Non-copyable (matches PhysicsSystem)
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    // System lifecycle
    void init(Application& app) override;
    void initDevice(); // standalone init without Application (for editor)
    void update(Application& app, float deltaTime) override;
    void updateStreaming(); // standalone update for streaming (for editor)
    void shutdown() override;

    // SFX API — preloaded WAV buffers (per D-11)
    uint32_t loadSound(const std::string& path);
    void playSound(uint32_t handle, const glm::vec3& position, float volume = 1.0f,
                   float pitch = 1.0f, float refDistance = 1.0f, float maxDistance = 50.0f);

    // Music streaming — OGG Vorbis via stb_vorbis (per D-12)
    void playMusic(const std::string& path, bool loop = true);
    void stopMusic();
    bool isMusicPlaying() const;

    // Ambient streaming — OGG Vorbis (per D-12)
    void playAmbient(const std::string& path, bool loop = true);
    void stopAmbient();
    bool isAmbientPlaying() const;

    // Volume categories (per D-02): float 0.0-1.0
    void setMasterVolume(float v);
    void setSfxVolume(float v);
    void setMusicVolume(float v);
    void setAmbientVolume(float v);
    float masterVolume() const;
    float sfxVolume() const;
    float musicVolume() const;
    float ambientVolume() const;

    // Listener transform — called each frame by AudioListenerSystem (per D-15)
    void setListenerTransform(const glm::vec3& position,
                              const glm::vec3& forward,
                              const glm::vec3& up);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
