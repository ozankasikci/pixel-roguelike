#include "game/rendering/EnvironmentDefinition.h"

#include "engine/core/PathUtils.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

[[noreturn]] void throwParseError(const std::string& path, int lineNumber, const std::string& message) {
    throw std::runtime_error(path + ":" + std::to_string(lineNumber) + ": " + message);
}

bool isCommentOrEmpty(const std::string& line) {
    for (char c : line) {
        if (c == '#') {
            return true;
        }
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

bool parseBool(const std::string& token, bool& value) {
    if (token == "1" || token == "true" || token == "TRUE") {
        value = true;
        return true;
    }
    if (token == "0" || token == "false" || token == "FALSE") {
        value = false;
        return true;
    }
    return false;
}

std::vector<std::string> tokenizeRecord(const std::string& line) {
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

glm::vec3 parseVec3Record(const std::vector<std::string>& tokens,
                          const std::string& path,
                          int lineNumber,
                          const std::string& label) {
    if (tokens.size() != 4) {
        throwParseError(path, lineNumber, "invalid " + label + " record");
    }
    return glm::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
}

float parseFloatRecord(const std::vector<std::string>& tokens,
                       const std::string& path,
                       int lineNumber,
                       const std::string& label) {
    if (tokens.size() != 2) {
        throwParseError(path, lineNumber, "invalid " + label + " record");
    }
    return std::stof(tokens[1]);
}

int parseIntRecord(const std::vector<std::string>& tokens,
                   const std::string& path,
                   int lineNumber,
                   const std::string& label) {
    if (tokens.size() != 2) {
        throwParseError(path, lineNumber, "invalid " + label + " record");
    }
    return std::stoi(tokens[1]);
}

bool parseBoolRecord(const std::vector<std::string>& tokens,
                     const std::string& path,
                     int lineNumber,
                     const std::string& label) {
    if (tokens.size() != 2) {
        throwParseError(path, lineNumber, "invalid " + label + " record");
    }
    bool value = false;
    if (!parseBool(tokens[1], value)) {
        throwParseError(path, lineNumber, "invalid " + label + " boolean");
    }
    return value;
}

std::string formatFloat(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    std::string text = stream.str();
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.push_back('0');
    }
    return text;
}

void writeVec3(std::ostream& out, const std::string& key, const glm::vec3& value) {
    out << key << ' '
        << formatFloat(value.x) << ' '
        << formatFloat(value.y) << ' '
        << formatFloat(value.z) << '\n';
}

void writeBool(std::ostream& out, const std::string& key, bool value) {
    out << key << ' ' << (value ? "true" : "false") << '\n';
}

void writeFloat(std::ostream& out, const std::string& key, float value) {
    out << key << ' ' << formatFloat(value) << '\n';
}

void writeInt(std::ostream& out, const std::string& key, int value) {
    out << key << ' ' << value << '\n';
}

void writeString(std::ostream& out, const std::string& key, const std::string& value) {
    out << key << ' ' << value << '\n';
}

void writeOptionalString(std::ostream& out, const std::string& key, const std::string& value) {
    if (!value.empty()) {
        writeString(out, key, value);
    }
}

void writeCubemapFaces(std::ostream& out, const std::array<std::string, 6>& faces) {
    if (faces[0].empty()) {
        return;
    }
    out << "sky_cubemap_faces";
    for (const auto& face : faces) {
        out << ' ' << face;
    }
    out << '\n';
}

} // namespace

EnvironmentDefinition makeEnvironmentDefinition(EnvironmentProfile profile) {
    const EnvironmentRenderSettings settings = makeEnvironmentRenderSettings(profile);
    EnvironmentDefinition definition;
    definition.id = environmentProfileName(profile);
    definition.post = settings.post;
    definition.sky = settings.sky;
    definition.lighting = settings.lighting;
    return definition;
}

EnvironmentRenderSettings makeEnvironmentRenderSettings(const EnvironmentDefinition& definition) {
    EnvironmentRenderSettings settings;
    settings.profile = EnvironmentProfile::Neutral;
    EnvironmentProfile parsedProfile = EnvironmentProfile::Neutral;
    if (tryParseEnvironmentProfileToken(definition.id, parsedProfile)) {
        settings.profile = parsedProfile;
    }
    settings.post = definition.post;
    settings.sky = definition.sky;
    settings.lighting = definition.lighting;
    return settings;
}

EnvironmentDefinition loadEnvironmentDefinitionAsset(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open environment definition: " + path);
    }

    EnvironmentDefinition definition = makeEnvironmentDefinition(EnvironmentProfile::Neutral);
    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        if (isCommentOrEmpty(line)) {
            continue;
        }

        const auto tokens = tokenizeRecord(line);
        if (tokens.empty()) {
            continue;
        }

        const std::string& key = tokens[0];
        if (key == "id" && tokens.size() == 2) {
            definition.id = tokens[1];
            EnvironmentProfile builtInProfile = EnvironmentProfile::Neutral;
            if (tryParseEnvironmentProfileToken(definition.id, builtInProfile)) {
                definition = makeEnvironmentDefinition(builtInProfile);
                definition.id = tokens[1];
            }
            continue;
        }

        if (key == "palette_variant") {
            definition.post.paletteVariant = parseIntRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "enable_sky") {
            definition.post.enableSky = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "enable_dither") {
            definition.post.enableDither = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "enable_edges") {
            definition.post.enableEdges = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "enable_fog") {
            definition.post.enableFog = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "enable_tone_map") {
            definition.post.enableToneMap = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "enable_bloom") {
            definition.post.enableBloom = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "enable_vignette") {
            definition.post.enableVignette = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "enable_grain") {
            definition.post.enableGrain = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "enable_scanlines") {
            definition.post.enableScanlines = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "enable_sharpen") {
            definition.post.enableSharpen = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "tone_map_mode") {
            definition.post.toneMapMode = parseIntRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "threshold_bias") {
            definition.post.thresholdBias = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "pattern_scale") {
            definition.post.patternScale = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "edge_threshold") {
            definition.post.edgeThreshold = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "fog_density") {
            definition.post.fogDensity = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "fog_start") {
            definition.post.fogStart = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "depth_view_scale") {
            definition.post.depthViewScale = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "exposure") {
            definition.post.exposure = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "gamma") {
            definition.post.gamma = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "contrast") {
            definition.post.contrast = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "saturation") {
            definition.post.saturation = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "bloom_threshold") {
            definition.post.bloomThreshold = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "bloom_intensity") {
            definition.post.bloomIntensity = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "bloom_radius") {
            definition.post.bloomRadius = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "vignette_strength") {
            definition.post.vignetteStrength = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "vignette_softness") {
            definition.post.vignetteSoftness = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "grain_amount") {
            definition.post.grainAmount = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "scanline_amount") {
            definition.post.scanlineAmount = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "scanline_density") {
            definition.post.scanlineDensity = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sharpen_amount") {
            definition.post.sharpenAmount = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "split_tone_strength") {
            definition.post.splitToneStrength = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "split_tone_balance") {
            definition.post.splitToneBalance = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "fog_near_color") {
            definition.post.fogNearColor = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "fog_far_color") {
            definition.post.fogFarColor = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "shadow_tint") {
            definition.post.shadowTint = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "highlight_tint") {
            definition.post.highlightTint = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }

        if (key == "sky_enabled") {
            definition.sky.enabled = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_zenith_color") {
            definition.sky.zenithColor = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_horizon_color") {
            definition.sky.horizonColor = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_ground_haze_color") {
            definition.sky.groundHazeColor = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_sun_direction") {
            definition.sky.sunDirection = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_sun_color") {
            definition.sky.sunColor = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_sun_size") {
            definition.sky.sunSize = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_sun_glow") {
            definition.sky.sunGlow = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_panorama_path" && tokens.size() == 2) {
            definition.sky.panoramaPath = tokens[1];
            continue;
        }
        if (key == "sky_panorama_tint") {
            definition.sky.panoramaTint = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_panorama_strength") {
            definition.sky.panoramaStrength = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_panorama_yaw_offset") {
            definition.sky.panoramaYawOffset = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_cubemap_faces" && tokens.size() == 7) {
            for (int i = 0; i < 6; ++i) {
                definition.sky.cubemapFacePaths[static_cast<std::size_t>(i)] = tokens[static_cast<std::size_t>(i + 1)];
            }
            continue;
        }
        if (key == "sky_cubemap_tint") {
            definition.sky.cubemapTint = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_cubemap_strength") {
            definition.sky.cubemapStrength = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_moon_enabled") {
            definition.sky.moonEnabled = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_moon_direction") {
            definition.sky.moonDirection = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_moon_color") {
            definition.sky.moonColor = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_moon_size") {
            definition.sky.moonSize = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_moon_glow") {
            definition.sky.moonGlow = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_cloud_layer_a" && tokens.size() == 2) {
            definition.sky.cloudLayerAPath = tokens[1];
            continue;
        }
        if (key == "sky_cloud_layer_b" && tokens.size() == 2) {
            definition.sky.cloudLayerBPath = tokens[1];
            continue;
        }
        if (key == "sky_horizon_layer" && tokens.size() == 2) {
            definition.sky.horizonLayerPath = tokens[1];
            continue;
        }
        if (key == "sky_cloud_tint") {
            definition.sky.cloudTint = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_cloud_scale") {
            definition.sky.cloudScale = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_cloud_speed") {
            definition.sky.cloudSpeed = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_cloud_coverage") {
            definition.sky.cloudCoverage = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_cloud_parallax") {
            definition.sky.cloudParallax = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_horizon_tint") {
            definition.sky.horizonTint = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_horizon_height") {
            definition.sky.horizonHeight = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sky_horizon_fade") {
            definition.sky.horizonFade = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }

        if (key == "lighting_hemi_sky_color") {
            definition.lighting.hemisphereSkyColor = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "lighting_hemi_ground_color") {
            definition.lighting.hemisphereGroundColor = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "lighting_hemi_strength") {
            definition.lighting.hemisphereStrength = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "lighting_enable_directional") {
            definition.lighting.enableDirectionalLights = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "lighting_enable_shadows") {
            definition.lighting.enableShadows = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "lighting_shadow_bias") {
            definition.lighting.shadowBias = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "lighting_shadow_normal_bias") {
            definition.lighting.shadowNormalBias = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sun_enabled") {
            definition.lighting.sun.enabled = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sun_direction") {
            definition.lighting.sun.direction = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sun_color") {
            definition.lighting.sun.color = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "sun_intensity") {
            definition.lighting.sun.intensity = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "fill_enabled") {
            definition.lighting.fill.enabled = parseBoolRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "fill_direction") {
            definition.lighting.fill.direction = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "fill_color") {
            definition.lighting.fill.color = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "fill_intensity") {
            definition.lighting.fill.intensity = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }

        throwParseError(path, lineNumber, "invalid environment definition record");
    }

    if (definition.id.empty()) {
        throw std::runtime_error("Environment definition missing id: " + path);
    }

    return definition;
}

std::string serializeEnvironmentDefinitionAsset(const EnvironmentDefinition& definition) {
    std::ostringstream out;

    writeString(out, "id", definition.id);
    writeInt(out, "palette_variant", definition.post.paletteVariant);
    writeBool(out, "enable_sky", definition.post.enableSky);
    writeBool(out, "enable_dither", definition.post.enableDither);
    writeBool(out, "enable_edges", definition.post.enableEdges);
    writeBool(out, "enable_fog", definition.post.enableFog);
    writeBool(out, "enable_tone_map", definition.post.enableToneMap);
    writeBool(out, "enable_bloom", definition.post.enableBloom);
    writeBool(out, "enable_vignette", definition.post.enableVignette);
    writeBool(out, "enable_grain", definition.post.enableGrain);
    writeBool(out, "enable_scanlines", definition.post.enableScanlines);
    writeBool(out, "enable_sharpen", definition.post.enableSharpen);
    writeInt(out, "tone_map_mode", definition.post.toneMapMode);
    writeFloat(out, "threshold_bias", definition.post.thresholdBias);
    writeFloat(out, "pattern_scale", definition.post.patternScale);
    writeFloat(out, "edge_threshold", definition.post.edgeThreshold);
    writeFloat(out, "fog_density", definition.post.fogDensity);
    writeFloat(out, "fog_start", definition.post.fogStart);
    writeFloat(out, "depth_view_scale", definition.post.depthViewScale);
    writeFloat(out, "exposure", definition.post.exposure);
    writeFloat(out, "gamma", definition.post.gamma);
    writeFloat(out, "contrast", definition.post.contrast);
    writeFloat(out, "saturation", definition.post.saturation);
    writeFloat(out, "bloom_threshold", definition.post.bloomThreshold);
    writeFloat(out, "bloom_intensity", definition.post.bloomIntensity);
    writeFloat(out, "bloom_radius", definition.post.bloomRadius);
    writeFloat(out, "vignette_strength", definition.post.vignetteStrength);
    writeFloat(out, "vignette_softness", definition.post.vignetteSoftness);
    writeFloat(out, "grain_amount", definition.post.grainAmount);
    writeFloat(out, "scanline_amount", definition.post.scanlineAmount);
    writeFloat(out, "scanline_density", definition.post.scanlineDensity);
    writeFloat(out, "sharpen_amount", definition.post.sharpenAmount);
    writeFloat(out, "split_tone_strength", definition.post.splitToneStrength);
    writeFloat(out, "split_tone_balance", definition.post.splitToneBalance);
    writeVec3(out, "fog_near_color", definition.post.fogNearColor);
    writeVec3(out, "fog_far_color", definition.post.fogFarColor);
    writeVec3(out, "shadow_tint", definition.post.shadowTint);
    writeVec3(out, "highlight_tint", definition.post.highlightTint);

    writeBool(out, "sky_enabled", definition.sky.enabled);
    writeVec3(out, "sky_zenith_color", definition.sky.zenithColor);
    writeVec3(out, "sky_horizon_color", definition.sky.horizonColor);
    writeVec3(out, "sky_ground_haze_color", definition.sky.groundHazeColor);
    writeVec3(out, "sky_sun_direction", definition.sky.sunDirection);
    writeVec3(out, "sky_sun_color", definition.sky.sunColor);
    writeFloat(out, "sky_sun_size", definition.sky.sunSize);
    writeFloat(out, "sky_sun_glow", definition.sky.sunGlow);
    writeOptionalString(out, "sky_panorama_path", definition.sky.panoramaPath);
    writeVec3(out, "sky_panorama_tint", definition.sky.panoramaTint);
    writeFloat(out, "sky_panorama_strength", definition.sky.panoramaStrength);
    writeFloat(out, "sky_panorama_yaw_offset", definition.sky.panoramaYawOffset);
    writeCubemapFaces(out, definition.sky.cubemapFacePaths);
    writeVec3(out, "sky_cubemap_tint", definition.sky.cubemapTint);
    writeFloat(out, "sky_cubemap_strength", definition.sky.cubemapStrength);
    writeBool(out, "sky_moon_enabled", definition.sky.moonEnabled);
    writeVec3(out, "sky_moon_direction", definition.sky.moonDirection);
    writeVec3(out, "sky_moon_color", definition.sky.moonColor);
    writeFloat(out, "sky_moon_size", definition.sky.moonSize);
    writeFloat(out, "sky_moon_glow", definition.sky.moonGlow);
    writeOptionalString(out, "sky_cloud_layer_a", definition.sky.cloudLayerAPath);
    writeOptionalString(out, "sky_cloud_layer_b", definition.sky.cloudLayerBPath);
    writeOptionalString(out, "sky_horizon_layer", definition.sky.horizonLayerPath);
    writeVec3(out, "sky_cloud_tint", definition.sky.cloudTint);
    writeFloat(out, "sky_cloud_scale", definition.sky.cloudScale);
    writeFloat(out, "sky_cloud_speed", definition.sky.cloudSpeed);
    writeFloat(out, "sky_cloud_coverage", definition.sky.cloudCoverage);
    writeFloat(out, "sky_cloud_parallax", definition.sky.cloudParallax);
    writeVec3(out, "sky_horizon_tint", definition.sky.horizonTint);
    writeFloat(out, "sky_horizon_height", definition.sky.horizonHeight);
    writeFloat(out, "sky_horizon_fade", definition.sky.horizonFade);

    writeVec3(out, "lighting_hemi_sky_color", definition.lighting.hemisphereSkyColor);
    writeVec3(out, "lighting_hemi_ground_color", definition.lighting.hemisphereGroundColor);
    writeFloat(out, "lighting_hemi_strength", definition.lighting.hemisphereStrength);
    writeBool(out, "lighting_enable_directional", definition.lighting.enableDirectionalLights);
    writeBool(out, "lighting_enable_shadows", definition.lighting.enableShadows);
    writeFloat(out, "lighting_shadow_bias", definition.lighting.shadowBias);
    writeFloat(out, "lighting_shadow_normal_bias", definition.lighting.shadowNormalBias);
    writeBool(out, "sun_enabled", definition.lighting.sun.enabled);
    writeVec3(out, "sun_direction", definition.lighting.sun.direction);
    writeVec3(out, "sun_color", definition.lighting.sun.color);
    writeFloat(out, "sun_intensity", definition.lighting.sun.intensity);
    writeBool(out, "fill_enabled", definition.lighting.fill.enabled);
    writeVec3(out, "fill_direction", definition.lighting.fill.direction);
    writeVec3(out, "fill_color", definition.lighting.fill.color);
    writeFloat(out, "fill_intensity", definition.lighting.fill.intensity);

    return out.str();
}

void saveEnvironmentDefinitionAsset(const std::string& path, const EnvironmentDefinition& definition) {
    namespace fs = std::filesystem;
    const fs::path target(resolveProjectPath(path));
    if (target.has_parent_path()) {
        fs::create_directories(target.parent_path());
    }

    std::ofstream out(target);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to write environment definition: " + target.string());
    }

    out << serializeEnvironmentDefinitionAsset(definition);
}
