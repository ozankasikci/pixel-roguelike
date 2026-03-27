#include "engine/rendering/assets/Texture2D.h"

#include <stb_image.h>

#include <utility>

Texture2D::~Texture2D() {
    destroy();
}

Texture2D::Texture2D(Texture2D&& other) noexcept
    : texture_(other.texture_) {
    other.texture_ = 0;
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept {
    if (this != &other) {
        destroy();
        texture_ = other.texture_;
        other.texture_ = 0;
    }
    return *this;
}

void Texture2D::createRGBA8(int width, int height, const std::vector<std::uint8_t>& pixels) {
    create(width, height, GL_RGBA8, GL_RGBA, pixels);
}

void Texture2D::createR8(int width, int height, const std::vector<std::uint8_t>& pixels) {
    create(width, height, GL_R8, GL_RED, pixels);
}

bool Texture2D::createRGBA8FromFile(const std::string& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (pixels == nullptr) {
        return false;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width * height * 4);
    std::vector<std::uint8_t> data(pixels, pixels + pixelCount);
    stbi_image_free(pixels);
    createRGBA8(width, height, data);
    return true;
}

bool Texture2D::createR8FromFile(const std::string& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, 1);
    if (pixels == nullptr) {
        return false;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width * height);
    std::vector<std::uint8_t> data(pixels, pixels + pixelCount);
    stbi_image_free(pixels);
    createR8(width, height, data);
    return true;
}

void Texture2D::destroy() {
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
}

void Texture2D::create(int width,
                       int height,
                       GLenum internalFormat,
                       GLenum format,
                       const std::vector<std::uint8_t>& pixels) {
    destroy();

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 internalFormat,
                 width,
                 height,
                 0,
                 format,
                 GL_UNSIGNED_BYTE,
                 pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}
