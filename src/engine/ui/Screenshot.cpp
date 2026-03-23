#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../external/stb_image_write.h"

#include "Screenshot.h"

#include <spdlog/spdlog.h>
#include <vector>
#include <ctime>
#include <cstring>

std::string saveScreenshot(int width, int height, const std::string& path) {
    std::string outPath = path;
    if (outPath.empty()) {
        // Generate timestamped filename
        char buf[128];
        std::time_t t = std::time(nullptr);
        std::strftime(buf, sizeof(buf), "screenshot_%Y%m%d_%H%M%S.png", std::localtime(&t));
        outPath = buf;
    }

    std::vector<unsigned char> pixels(width * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // OpenGL reads bottom-to-top; flip for PNG (top-to-bottom)
    int rowSize = width * 3;
    std::vector<unsigned char> flipped(pixels.size());
    for (int y = 0; y < height; ++y) {
        std::memcpy(&flipped[y * rowSize], &pixels[(height - 1 - y) * rowSize], rowSize);
    }

    int result = stbi_write_png(outPath.c_str(), width, height, 3, flipped.data(), rowSize);
    if (result) {
        spdlog::info("Screenshot saved: {}", outPath);
    } else {
        spdlog::error("Failed to save screenshot: {}", outPath);
    }

    return outPath;
}

void AutoScreenshot::enable(const std::string& outputPath, int delayFrames) {
    enabled_ = true;
    done_ = false;
    outputPath_ = outputPath;
    delayFrames_ = delayFrames;
    frameCount_ = 0;
}

bool AutoScreenshot::tick(int windowWidth, int windowHeight) {
    if (!enabled_ || done_) return false;

    frameCount_++;
    if (frameCount_ >= delayFrames_) {
        saveScreenshot(windowWidth, windowHeight, outputPath_);
        done_ = true;
        return true;
    }
    return false;
}
