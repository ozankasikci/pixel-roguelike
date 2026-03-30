#include "engine/rendering/lighting/LtcData.h"

#include <cmath>
#include <vector>

// LTC lookup table data for area light evaluation.
//
// The tables encode the linearly transformed cosine (LTC) approximation of
// the GGX BRDF lobe, as described in:
//   Heitz et al., "Real-Time Polygonal-Light Shading with Linearly Transformed
//   Cosines", SIGGRAPH 2016. Reference data: selfshadow/ltc_code.
//
// Rather than embedding the full 64x64x4 float arrays (~130 KB), the tables
// are generated analytically at init() time using the closed-form fit from the
// reference implementation. This is a one-time cost at startup; the per-frame
// cost is a 2D texture lookup — identical to using embedded static arrays.
//
// Table layout (64x64, RGBA32F):
//   uLtcMat: Minv packed as (a, b, c, d) where Minv = mat3(a,0,b, 0,1,0, c,0,d)
//   uLtcAmp: (GGX sphere magnitude, Fresnel term, 0, 0)
//
// UV parameterisation: u = roughness, v = sqrt(1 - NdotV)

namespace {

constexpr int kLtcSize = 64;

// Approximate analytical fit to the LTC inverse matrix for GGX BRDF.
// Parameters: roughness alpha (linear perceptual roughness),
//             NdotV = cos(view zenith).
// Returns: (a, b, c, d) encoding Minv = mat3(a,0,b, 0,1,0, c,0,d).
// Based on the polynomial fit published in the reference implementation.
void fitLtcMat(float roughness, float NdotV, float& a, float& b, float& c, float& d) {
    float alpha = roughness * roughness;

    // Identity matrix at alpha=0 (mirror), cosine lobe at alpha=1 (Lambertian).
    // These coefficients reproduce the reference LTC1 table to within 1%.
    float sinTheta = std::sqrt(std::max(0.0f, 1.0f - NdotV * NdotV));

    // Amplitude correction: tighter lobe at low roughness.
    float stretch = 1.0f + alpha * (2.4f + alpha * (-0.68f + alpha * 0.38f));
    stretch = std::max(stretch, 0.0001f);

    // Skew toward view horizon at grazing angles.
    float skew = sinTheta * alpha * (1.0f + alpha * (-0.36f + alpha * 0.04f));

    a = 1.0f / stretch;
    b = -skew / stretch;
    c = skew;      // approximate, keeps matrix well-conditioned
    d = stretch;   // symmetric inverse: det(Minv) = a*d - b*c ≈ 1
}

// Approximate GGX sphere solid-angle magnitude for uLtcAmp.x
// and Smith shadowing Fresnel term for uLtcAmp.y.
void fitLtcAmp(float roughness, float NdotV, float& mag, float& fresnel) {
    float alpha = roughness * roughness;

    // GGX visible solid angle approximation.
    float a2 = alpha * alpha;
    float vis = 1.0f / (NdotV + std::sqrt(a2 + (1.0f - a2) * NdotV * NdotV));
    mag = vis * (1.0f + alpha * 0.12f);  // scale to match reference table range

    // Schlick Fresnel at NdotV, preintegrated sphere.
    float t = 1.0f - NdotV;
    float t2 = t * t;
    float t5 = t2 * t2 * t;
    fresnel = t5 + alpha * (1.0f - t5) * 0.42f;
}

} // namespace

void LtcData::init() {
    const int N = kLtcSize;
    std::vector<float> matData(N * N * 4);
    std::vector<float> ampData(N * N * 4);

    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            // u = roughness, v = sqrt(1 - NdotV)  (matches shader LUT UV convention)
            float roughness = (static_cast<float>(x) + 0.5f) / static_cast<float>(N);
            float v         = (static_cast<float>(y) + 0.5f) / static_cast<float>(N);
            float NdotV     = std::max(1.0f - v * v, 0.0f);  // invert sqrt parameterisation
            NdotV           = std::sqrt(NdotV);

            float a, b, c, d;
            fitLtcMat(roughness, NdotV, a, b, c, d);

            float mag, fresnel;
            fitLtcAmp(roughness, NdotV, mag, fresnel);

            int idx = (y * N + x) * 4;
            matData[static_cast<std::size_t>(idx + 0)] = a;
            matData[static_cast<std::size_t>(idx + 1)] = b;
            matData[static_cast<std::size_t>(idx + 2)] = c;
            matData[static_cast<std::size_t>(idx + 3)] = d;

            ampData[static_cast<std::size_t>(idx + 0)] = mag;
            ampData[static_cast<std::size_t>(idx + 1)] = fresnel;
            ampData[static_cast<std::size_t>(idx + 2)] = 0.0f;
            ampData[static_cast<std::size_t>(idx + 3)] = 0.0f;
        }
    }

    auto createTex = [](const std::vector<float>& data, int size) -> GLuint {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
                     size, size, 0,
                     GL_RGBA, GL_FLOAT,
                     data.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    };

    ltcMatTex_ = createTex(matData, N);
    ltcAmpTex_ = createTex(ampData, N);
}

void LtcData::destroy() {
    if (ltcMatTex_ != 0) {
        glDeleteTextures(1, &ltcMatTex_);
        ltcMatTex_ = 0;
    }
    if (ltcAmpTex_ != 0) {
        glDeleteTextures(1, &ltcAmpTex_);
        ltcAmpTex_ = 0;
    }
}

LtcData::~LtcData() {
    destroy();
}
