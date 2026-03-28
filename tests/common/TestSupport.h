#pragma once

#include <cmath>
#include <filesystem>
#include <string_view>

#include <glm/glm.hpp>

namespace test_support {

inline constexpr float kFloatEpsilon = 0.0001f;

inline bool nearlyEqual(float a, float b, float epsilon = kFloatEpsilon) {
    return std::fabs(a - b) <= epsilon;
}

inline bool nearlyEqualVec3(const glm::vec3& a, const glm::vec3& b, float epsilon = kFloatEpsilon) {
    return nearlyEqual(a.x, b.x, epsilon)
        && nearlyEqual(a.y, b.y, epsilon)
        && nearlyEqual(a.z, b.z, epsilon);
}

inline std::filesystem::path tempPath(std::string_view leaf) {
    return std::filesystem::temp_directory_path() / std::filesystem::path(std::string(leaf));
}

inline std::filesystem::path resetTempDirectory(std::string_view leaf) {
    const auto path = tempPath(leaf);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

} // namespace test_support
