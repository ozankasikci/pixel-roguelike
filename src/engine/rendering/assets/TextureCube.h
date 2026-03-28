#pragma once

#include <glad/gl.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class TextureCube {
public:
    TextureCube() = default;
    ~TextureCube();

    TextureCube(const TextureCube&) = delete;
    TextureCube& operator=(const TextureCube&) = delete;

    TextureCube(TextureCube&& other) noexcept;
    TextureCube& operator=(TextureCube&& other) noexcept;

    void createRGBA8(int width,
                     int height,
                     const std::array<std::vector<std::uint8_t>, 6>& facePixels);
    bool createRGBA8FromFiles(const std::array<std::string, 6>& paths);

    GLuint id() const { return texture_; }

private:
    void destroy();

    GLuint texture_ = 0;
};
