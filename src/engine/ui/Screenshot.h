#pragma once

#include <glad/gl.h>
#include <string>

// Save current framebuffer to PNG file. Returns the path written.
std::string saveScreenshot(int width, int height, const std::string& path = "");

// Auto-screenshot: captures after N frames, saves to given path, then sets a flag.
// Used for automated testing — run the game, wait for it to render, capture, exit.
class AutoScreenshot {
public:
    AutoScreenshot() = default;

    // Enable auto-capture: after `delayFrames` frames, save screenshot and set done=true
    void enable(const std::string& outputPath, int delayFrames = 5);

    // Call every frame. Returns true when screenshot was just taken.
    bool tick(int windowWidth, int windowHeight);

    bool isDone() const { return done_; }

private:
    bool enabled_ = false;
    bool done_ = false;
    std::string outputPath_;
    int delayFrames_ = 5;
    int frameCount_ = 0;
};
