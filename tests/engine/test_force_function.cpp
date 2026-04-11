#include "engine/particles/ForceFunction.h"
#include <cassert>
#include <cmath>

static bool nearEq(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

int main() {
    using namespace particles;

    // Gravity
    { GravityForce g(glm::vec3(0,-10,0)); auto r=g.apply({},{1,0,0},0.1f); assert(nearEq(r.x,1.0f)); assert(nearEq(r.y,-1.0f)); assert(nearEq(r.z,0.0f)); }

    // Drag
    { DragForce d(2.0f); auto r=d.apply({},{10,0,0},0.1f); assert(nearEq(r.x,8.0f)); }

    // Drag clamped
    { DragForce d(100.0f); auto r=d.apply({},{5,5,5},0.1f); assert(nearEq(r.x,0.0f)); assert(nearEq(r.y,0.0f)); assert(nearEq(r.z,0.0f)); }

    return 0;
}
