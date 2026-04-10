#include "engine/rendering/RenderQualitySettings.h"

#include <cassert>

int main() {
    static_assert(kRenderResolutionPresets.size() == 5);
    static_assert(kDefaultInternalResolutionIndex == 3);

    assert(kRenderResolutionPresets[0].width == 854);
    assert(kRenderResolutionPresets[0].height == 480);
    assert(kRenderResolutionPresets[3].width == 1920);
    assert(kRenderResolutionPresets[3].height == 1080);
    assert(kRenderResolutionPresets[4].width == 2560);
    assert(kRenderResolutionPresets[4].height == 1440);

    for (std::size_t i = 1; i < kRenderResolutionPresets.size(); ++i) {
        assert(kRenderResolutionPresets[i - 1].width < kRenderResolutionPresets[i].width);
        assert(kRenderResolutionPresets[i - 1].height < kRenderResolutionPresets[i].height);
    }

    assert(resolveTextureAnisotropyLevel(false, 16.0f) == 1.0f);
    assert(resolveTextureAnisotropyLevel(true, 0.0f) == 1.0f);
    assert(resolveTextureAnisotropyLevel(true, 2.0f) == 2.0f);
    assert(resolveTextureAnisotropyLevel(true, 16.0f) == kPreferredTextureAnisotropy);

    return 0;
}
