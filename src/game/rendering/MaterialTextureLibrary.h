#pragma once

#include "engine/rendering/assets/Texture2D.h"
#include "engine/rendering/geometry/Renderer.h"

#include <array>
#include <string_view>
#include <unordered_map>

class ContentRegistry;

class MaterialTextureLibrary {
public:
    void init(const ContentRegistry& content);

    const RenderMaterialData& resolve(std::string_view materialId, MaterialKind legacyKind) const;

private:
    struct TextureSet {
        Texture2D albedo;
        Texture2D normal;
        Texture2D roughness;
        Texture2D ao;
    };

    void createFallbackTextures();
    void buildBrickSet(TextureSet& brick) const;
    void buildStoneSet(TextureSet& stone) const;

    TextureSet fallbackTextures_{};
    std::unordered_map<std::string, TextureSet> textureSets_{};
    std::unordered_map<std::string, RenderMaterialData> materials_{};
};
