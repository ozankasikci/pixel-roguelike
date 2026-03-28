#include "engine/rendering/assets/TextureCube.h"

#include <stb_image.h>

#include <utility>

TextureCube::~TextureCube() {
    destroy();
}

TextureCube::TextureCube(TextureCube&& other) noexcept
    : texture_(other.texture_) {
    other.texture_ = 0;
}

TextureCube& TextureCube::operator=(TextureCube&& other) noexcept {
    if (this != &other) {
        destroy();
        texture_ = other.texture_;
        other.texture_ = 0;
    }
    return *this;
}

void TextureCube::createRGBA8(int width,
                              int height,
                              const std::array<std::vector<std::uint8_t>, 6>& facePixels) {
    destroy();

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    for (std::size_t faceIndex = 0; faceIndex < facePixels.size(); ++faceIndex) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(faceIndex),
                     0,
                     GL_RGBA8,
                     width,
                     height,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     facePixels[faceIndex].data());
    }

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

bool TextureCube::createRGBA8FromFiles(const std::array<std::string, 6>& paths) {
    std::array<std::vector<std::uint8_t>, 6> facePixels;
    int width = 0;
    int height = 0;

    for (std::size_t faceIndex = 0; faceIndex < paths.size(); ++faceIndex) {
        int faceWidth = 0;
        int faceHeight = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load(paths[faceIndex].c_str(), &faceWidth, &faceHeight, &channels, 4);
        if (pixels == nullptr) {
            return false;
        }

        if (faceIndex == 0) {
            width = faceWidth;
            height = faceHeight;
        } else if (faceWidth != width || faceHeight != height) {
            stbi_image_free(pixels);
            return false;
        }

        const std::size_t pixelCount = static_cast<std::size_t>(faceWidth * faceHeight * 4);
        facePixels[faceIndex].assign(pixels, pixels + pixelCount);
        stbi_image_free(pixels);
    }

    createRGBA8(width, height, facePixels);
    return true;
}

void TextureCube::destroy() {
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
}
