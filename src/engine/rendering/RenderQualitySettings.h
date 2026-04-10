#pragma once

#include <array>
#include <algorithm>

struct RenderResolutionPreset {
    int width = 1280;
    int height = 720;
    const char* label = "720p (1280x720)";
};

inline constexpr std::array<RenderResolutionPreset, 5> kRenderResolutionPresets{{
    {854, 480, "480p (854x480)"},
    {960, 540, "540p (960x540)"},
    {1280, 720, "720p (1280x720)"},
    {1920, 1080, "1080p (1920x1080)"},
    {2560, 1440, "1440p (2560x1440)"},
}};

inline constexpr int kDefaultInternalResolutionIndex = 3;
inline constexpr float kPreferredTextureAnisotropy = 8.0f;

constexpr float resolveTextureAnisotropyLevel(bool supported, float deviceMaxAnisotropy) {
    if (!supported || deviceMaxAnisotropy < 1.0f) {
        return 1.0f;
    }
    return std::clamp(kPreferredTextureAnisotropy, 1.0f, deviceMaxAnisotropy);
}
