#pragma once

#include <glad/gl.h>

// LtcData owns two 64x64 RGBA32F GPU textures derived from the LTC lookup
// tables published by Heitz et al., "Real-Time Polygonal-Light Shading with
// Linearly Transformed Cosines" (2016), selfshadow/ltc_code.
//
// - ltcMatTex_: packed inverse LTC transform matrix (a, b, c, d) per texel
//   such that Minv = mat3(a,0,b, 0,1,0, c,0,d)
// - ltcAmpTex_: GGX magnitude (x), Fresnel term (y) per texel; .z and .w are unused
//
// UV: (roughness, sqrt(1 - NdotV)) mapped to (0..1, 0..1) clamped to 63/64..

class LtcData {
public:
    LtcData() = default;
    ~LtcData();

    LtcData(const LtcData&) = delete;
    LtcData& operator=(const LtcData&) = delete;

    void init();
    void destroy();

    GLuint ltcMatTexture() const { return ltcMatTex_; }
    GLuint ltcAmpTexture() const { return ltcAmpTex_; }

private:
    GLuint ltcMatTex_ = 0;  // 64x64 RGBA32F — inverse transform matrix (a, 0, b, d)
    GLuint ltcAmpTex_ = 0;  // 64x64 RGBA32F — GGX magnitude + Fresnel
};
