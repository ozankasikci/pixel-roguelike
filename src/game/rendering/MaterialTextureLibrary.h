#pragma once

#include "engine/rendering/assets/Texture2D.h"
#include "engine/rendering/geometry/Renderer.h"
#include "game/rendering/MaterialDefinition.h"

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>

class ContentRegistry;

class MaterialTextureLibrary {
public:
    void init(const ContentRegistry& content);
    std::size_t prewarmAllMaterialMaps() const;

    const RenderMaterialData& resolve(std::string_view materialId, MaterialKind legacyKind) const;

private:
    struct TextureSet {
        Texture2D albedo;
        Texture2D normal;
        Texture2D roughness;
        Texture2D ao;
    };

    void createFallbackTextures();
    const ResolvedMaterialDefinition& definitionFor(std::string_view materialId, MaterialKind legacyKind) const;
    const TextureSet& ensureTextureSet(const ResolvedMaterialDefinition& resolved) const;
    std::string textureKeyFor(const ResolvedMaterialDefinition& resolved) const;
    struct ProceduralPixelData {
        std::vector<std::uint8_t> albedo;     // RGBA8, size*size*4
        std::vector<std::uint8_t> normal;     // RGBA8, size*size*4
        std::vector<std::uint8_t> roughness;  // R8, size*size
        std::vector<std::uint8_t> ao;         // R8, size*size
        int size;
    };

    void buildBrickSet(TextureSet& brick) const;
    void buildStoneSet(TextureSet& stone) const;
    ProceduralPixelData generateBrickPixels() const;
    ProceduralPixelData generateStonePixels() const;
    ProceduralPixelData generateSmoothWallPixels() const;
    ProceduralPixelData generateFloorPixels() const;
    ProceduralPixelData generateCeilingPixels() const;

    TextureSet fallbackTextures_{};
    std::unordered_map<std::string, ResolvedMaterialDefinition> resolvedDefinitions_{};
    mutable std::unordered_map<std::string, TextureSet> textureSets_{};
    mutable std::unordered_map<std::string, RenderMaterialData> materials_{};
};
