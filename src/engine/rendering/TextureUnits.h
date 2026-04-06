#pragma once

// Scene shader texture unit assignments.
//
// Units 0-2 are reserved for global/local environment reflection resources.
// Units 8-15 are used for shadow maps (bound once per frame before the draw loop).
// Units 12-15 are ALSO used for per-object material maps (albedo/normal/roughness/ao)
// bound within the draw loop -- overwriting shadow map bindings for units 12-15.
// This overlap is intentional: shadow maps are sampled via separate uniforms
// (uShadowMaps[]) while material maps are sampled via uAlbedoMap etc.
// Units 10-11 are LTC lookup tables, bound once per scene pass.
// Unit 16 is the CSM depth array.
namespace TextureUnits {
    constexpr int kEnvironmentSpecular = 0;
    constexpr int kEnvironmentBrdfLut  = 1;
    constexpr int kReflectionProbeMap  = 2;
// Shadow map slots (6 shadow lights max, from RenderLight.h kMaxShadowedSpotLights)
    constexpr int kShadowMap0     = 8;   // through kShadowMap0 + 5 = 13
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
