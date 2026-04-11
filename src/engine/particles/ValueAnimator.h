#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace particles {

template<typename T>
class ValueAnimator {
public:
    virtual ~ValueAnimator() = default;
    virtual T evaluate(float t) const = 0;
};

template<typename T>
class Constant : public ValueAnimator<T> {
public:
    explicit Constant(T value) : value_(value) {}
    T evaluate(float) const override { return value_; }
private:
    T value_;
};

template<typename T>
class Lerp : public ValueAnimator<T> {
public:
    Lerp(T start, T end) : start_(start), end_(end) {}
    T evaluate(float t) const override {
        const float clamped = glm::clamp(t, 0.0f, 1.0f);
        return start_ + (end_ - start_) * clamped;
    }
private:
    T start_;
    T end_;
};

class ColorGradient : public ValueAnimator<glm::vec4> {
public:
    using Stop = std::pair<float, glm::vec4>;

    explicit ColorGradient(std::vector<Stop> stops) : stops_(std::move(stops)) {
        std::sort(stops_.begin(), stops_.end(),
                  [](const Stop& a, const Stop& b) { return a.first < b.first; });
    }

    glm::vec4 evaluate(float t) const override {
        if (stops_.empty()) return glm::vec4(1.0f);
        if (stops_.size() == 1 || t <= stops_.front().first) return stops_.front().second;
        if (t >= stops_.back().first) return stops_.back().second;

        for (std::size_t i = 0; i + 1 < stops_.size(); ++i) {
            if (t >= stops_[i].first && t <= stops_[i + 1].first) {
                const float range = stops_[i + 1].first - stops_[i].first;
                const float local = (range > 0.0f) ? (t - stops_[i].first) / range : 0.0f;
                return glm::mix(stops_[i].second, stops_[i + 1].second, local);
            }
        }
        return stops_.back().second;
    }

private:
    std::vector<Stop> stops_;
};

class FloatCurve : public ValueAnimator<float> {
public:
    using Keyframe = std::pair<float, float>;

    explicit FloatCurve(std::vector<Keyframe> keyframes) : keyframes_(std::move(keyframes)) {
        std::sort(keyframes_.begin(), keyframes_.end(),
                  [](const Keyframe& a, const Keyframe& b) { return a.first < b.first; });
    }

    float evaluate(float t) const override {
        if (keyframes_.empty()) return 0.0f;
        if (keyframes_.size() == 1 || t <= keyframes_.front().first) return keyframes_.front().second;
        if (t >= keyframes_.back().first) return keyframes_.back().second;

        for (std::size_t i = 0; i + 1 < keyframes_.size(); ++i) {
            if (t >= keyframes_[i].first && t <= keyframes_[i + 1].first) {
                const float range = keyframes_[i + 1].first - keyframes_[i].first;
                const float local = (range > 0.0f) ? (t - keyframes_[i].first) / range : 0.0f;
                return glm::mix(keyframes_[i].second, keyframes_[i + 1].second, local);
            }
        }
        return keyframes_.back().second;
    }

private:
    std::vector<Keyframe> keyframes_;
};

} // namespace particles
