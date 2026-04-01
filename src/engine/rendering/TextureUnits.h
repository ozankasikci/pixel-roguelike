#pragma once

// Scene shader texture unit assignments.
//
// Units 8-15 are used for shadow maps (bound once per frame before the draw loop).
// Units 12-15 are ALSO used for per-object material maps (albedo/normal/roughness/ao)
// bound within the draw loop -- overwriting the shadow map bindings for each object.
// This overlap is intentional: shadow maps are sampled via separate uniforms
// (uShadowMaps[]) while material maps are sampled via uAlbedoMap etc.
// Units 10-11 are LTC lookup tables, bound once per scene pass.
// Unit 16 is the CSM depth array.
namespace TextureUnits {
    // Shadow map slots (8 shadow lights max, from RenderLight.h kMaxShadowedSpotLights)
    constexpr int kShadowMap0     = 8;   // through kShadowMap0 + 7 = 15
    // LTC area-light lookup tables (bound once per scene pass, sit within shadow range)
    constexpr int kLtcMat         = 10;
    constexpr int kLtcAmp         = 11;
    // Per-object material maps (overwrite shadow slots 12-15 per draw call)
    constexpr int kAlbedo         = 12;
    constexpr int kNormalMap      = 13;
    constexpr int kRoughnessMap   = 14;
    constexpr int kAoMap          = 15;
    // Cascaded shadow map (directional sun)
    constexpr int kCsmShadowMap   = 16;
} // namespace TextureUnits
