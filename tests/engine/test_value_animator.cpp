#include "engine/particles/ValueAnimator.h"

#include <cassert>
#include <cmath>

static bool nearEq(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }
static bool nearEqVec4(const glm::vec4& a, const glm::vec4& b, float eps = 1e-4f) {
    return nearEq(a.x, b.x, eps) && nearEq(a.y, b.y, eps) && nearEq(a.z, b.z, eps) && nearEq(a.w, b.w, eps);
}

int main() {
    using namespace particles;

    // Constant<float>
    { Constant<float> c(5.0f); assert(nearEq(c.evaluate(0.0f), 5.0f)); assert(nearEq(c.evaluate(1.0f), 5.0f)); }

    // Lerp<float>
    { Lerp<float> l(0.0f, 10.0f); assert(nearEq(l.evaluate(0.0f), 0.0f)); assert(nearEq(l.evaluate(0.5f), 5.0f)); assert(nearEq(l.evaluate(1.0f), 10.0f)); assert(nearEq(l.evaluate(-0.5f), 0.0f)); assert(nearEq(l.evaluate(1.5f), 10.0f)); }

    // Lerp<vec4>
    { Lerp<glm::vec4> l(glm::vec4(1,0,0,1), glm::vec4(0,0,1,0)); assert(nearEqVec4(l.evaluate(0.5f), glm::vec4(0.5f,0,0.5f,0.5f))); }

    // ColorGradient
    { ColorGradient g({{0.0f, glm::vec4(1,1,0,1)}, {0.5f, glm::vec4(1,0,0,1)}, {1.0f, glm::vec4(0,0,0,0)}}); assert(nearEqVec4(g.evaluate(0.0f), glm::vec4(1,1,0,1))); assert(nearEqVec4(g.evaluate(0.25f), glm::vec4(1,0.5f,0,1))); assert(nearEqVec4(g.evaluate(0.5f), glm::vec4(1,0,0,1))); assert(nearEqVec4(g.evaluate(1.0f), glm::vec4(0,0,0,0))); }

    // ColorGradient unsorted stops
    { ColorGradient g({{1.0f, glm::vec4(0)}, {0.0f, glm::vec4(1)}}); assert(nearEqVec4(g.evaluate(0.5f), glm::vec4(0.5f))); }

    // ColorGradient edge cases
    { ColorGradient empty({}); assert(nearEqVec4(empty.evaluate(0.5f), glm::vec4(1.0f))); ColorGradient single({{0.5f, glm::vec4(0.3f,0.6f,0.9f,1.0f)}}); assert(nearEqVec4(single.evaluate(0.0f), glm::vec4(0.3f,0.6f,0.9f,1.0f))); }

    // FloatCurve
    { FloatCurve curve({{0.0f,0.1f},{0.5f,1.0f},{1.0f,0.0f}}); assert(nearEq(curve.evaluate(0.0f), 0.1f)); assert(nearEq(curve.evaluate(0.25f), 0.55f)); assert(nearEq(curve.evaluate(0.5f), 1.0f)); assert(nearEq(curve.evaluate(0.75f), 0.5f)); assert(nearEq(curve.evaluate(1.0f), 0.0f)); }

    // FloatCurve edge cases
    { FloatCurve empty({}); assert(nearEq(empty.evaluate(0.5f), 0.0f)); FloatCurve single({{0.5f, 3.0f}}); assert(nearEq(single.evaluate(0.0f), 3.0f)); }

    return 0;
}
