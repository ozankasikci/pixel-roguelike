#pragma once

#include <glad/gl.h>
#include <cstdint>
#include <string>
#include <vector>

class Texture2D {
public:
    Texture2D() = default;
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;

    void createRGBA8(int width, int height, const std::vector<std::uint8_t>& pixels);
    void createR8(int width, int height, const std::vector<std::uint8_t>& pixels);
    bool createRGBA8FromFile(const std::string& path);
    bool createR8FromFile(const std::string& path);

    GLuint id() const { return texture_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    void destroy();
    void create(int width,
                int height,
                GLenum internalFormat,
                GLenum format,
                const std::vector<std::uint8_t>& pixels);

    GLuint texture_ = 0;
    int width_ = 0;
    int height_ = 0;
};
